# Key Concepts in Chrome Memory

## What's so hard about memory? Isn't it just malloc and free?

Not really. There are lots of differences and subtleties that change per
operating system and even per operating system configuration.

Fortunately, these differences mostly disappear when a program is running
with sufficient resources.

Unfortunately, the distinctions end up being very relevant when
working near out of memory conditions or analyzing overall performance
when there is any amount of memory pressure; this makes crafting and
interpreting memory statistics hard.

Fortunately, the point of this doc is to give succinct background that
will help you ramp up on the subtleties to work in this space. Yes, this
is complicated stuff...but don't despair. You work on a multi-process
browser implementing the web platform with high security guarantees.
Compared to the rest the system, memory is not THAT complicated.

## Can you give specific examples of how it's harder than malloc/free?

Here are some example questions that require a more complex
view of memory than malloc/free.

  * When Chrome allocates memory, when does it take up swap space?
  * When memory is `free()`d, when is it made usable by other applications?
  * Is it always safe to touch the memory returned by malloc()?
  * How many heaps does Chrome have?
  * How are memory resources used by the GPU and drivers accounted for?
  * Is that the same on systems where GPU memory isn't shared with main memory?
  * How are shared libraries accounted for? How big of a penalty is there for
    each process that shares the memory?
  * What types of memory does Task Manager/Activity Monitor/top report?
  * What about the UMA stats?

In many of the above, the answer actually changes per operating system variant.
There is at least one major schism between Windows-based machines and more
unixy systems. For example, it is impossible to return all resources (physical
ram as well as swap space) to the OS in a way brings them back on demand which
drastically changes the way one can handle free lists.

However, even in macOS, Android, CrOS, and "standard desktop linux" each
also have enough divergences (compressed memory, pagefile vs swap partition
vs no swap, overcommit settings, memory perssure signals etc) that even
answering "how much memory is Chromium using" is hard to do in a uniform
manner.

The goal of this document is to give a common set of vocabulary
and concepts such that Chromium developers can more discuss questions like
the ones above without misunderstanding each other.


## Key gotchas

### Windows allocation uses resources immediately; other OSes use it on first touch.

Arguably the biggest difference for Windows and other OSes is memory granted to
a process is always "committed" on allocation. Pragmatically this means that in
Windows, `malloc(10*1024*1024*1024)` will immediately prevent other applications
from being able to successfully allocate memory thereby causing them to crash
or not be able to open. In Unix variants, usage usually only consumes system
resources [TODO(awong): Link to overcommit] when pages are touched.

Not being aware of this difference can cause architecture choices that have a
larger than expected resource impact on Windows and incorrect interpretation for metrics on Windows

See the following section on "discardable" memory for more info.


### Because of the commit guarantee difference, "discarding" memory has completely different meanings across platforms.

In Unix systems, there is an `madvise()` function via which pages that have
been committed via usage can be returned to the non-resource consuming state.
Such a page will then be recommitted on demand making it a tempting optimization
for data structures with freelists. However, there is no such API on Windows.
The `VirtualAlloc(MEM_RESET)`, `DiscardVirtualMemory()`, and
`OfferVirtualMemory()` look temptingly similar and on first glance they even
look like they work because they will immediately reduce the amount of physical
ram (aka Working Set) a processes uses. However, they do NOT release swap
meaning they will not help prevent OOM scenarios.

Designing a freelist structure that conflates this behavior (see this
[PartitionAlloc bug](https://bugs.chromium.org/p/chromium/issues/detail?id=726077))
will result in a system that only truly reduces resource usage on Unix-like
systems.


## Terms and definitions

TODO(awong): To through Erik's Consistent Memory Metrics doc and pull out bits
that reconcile with this.

### Commited Memory
### Discardable memory
### Proportional Set Size
### Image memory
### Shared Memory.

TODO(awong): Write overview of our platform diversity, windows vs \*nix memory models (eg,
"committed" memory), what "discardable" memory is, GPU memory, zram, overcommit,
the various Chrome heaps (pageheap, partitionalloc, oilpan, v8, malloc...per
platform), etc.
