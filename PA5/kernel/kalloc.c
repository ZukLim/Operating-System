// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#ifdef SNU
uint64 freemem = 0;
#endif

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

void *ref_start = 0;
void *pa_start = 0;

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

uint32 *get_refP(void *pa) {
	uint32 *refP = ref_start;
	return refP + (pa - pa_start) / PGSIZE;
}

uint32 ref_up(void *pa) {
	uint32 ref = ++(*get_refP(pa));
	return ref;
}

uint32 ref_down(void *pa) {
	uint32 ref = --(*get_refP(pa));
	return ref;
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  uint64 ref_space = ((uint64) PHYSTOP - (uint64) end) / PGSIZE * sizeof(uint32);
  ref_start = (void *) PGROUNDUP((uint64) end);
  pa_start = (void *) PGROUNDUP((uint64) ref_start + ref_space);
  memset(ref_start, 0, ref_space);
  freerange(pa_start, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
	ref_up(p);
    kfree(p);
  }
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  if(ref_down(pa)) return;

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
#ifdef SNU
  freemem++;
#endif
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
#ifdef SNU
  {
    kmem.freelist = r->next;
    freemem--;
	ref_up(r);
  }
#else
    kmem.freelist = r->next;
#endif    
  release(&kmem.lock);
  
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
