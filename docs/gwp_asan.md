# GWP-ASan

GWP-ASan is a debug tool intended to detect heap memory errors in the wild. It
samples alloctions to a debug allocator, similar to ElectricFence or Page Heap,
in order to detect heap memory errors and report additional debugging context.

## Allocator

The GuardedPageAllocator returns allocations on pages buffered on both sides by
guard pages. The allocations are either left- or right-aligned to detect buffer
overflows and underflows. When an allocation is freed, the page is marked
inaccessible so use-after-frees cause an exception (until that page is reused
for another allocation.)

The allocator saves stack traces on every allocation and deallocation to
preserve debug context if that allocation results in a memory error.

Allocations are sampled to the GuardedPageAllocator using an [allocator shim.](/base/allocator/README.md)

## Crash handler

The allocator is designed so that memory errors with GWP-ASan allocations
intentionally trigger invalid access exceptions. A hook in the crashpad crash
handler process inspects crashes, determines if they are GWP-ASan exceptions,
and adds additional debug information to the crash minidump if so.

The crash handler hook determines if the exception was related to GWP-ASan by
reading the allocator internals and seeing if the exception address was within
the bounds of the allocator region. If it is, the crash handler hook extracts
debug information about that allocation, such as thread IDs and stack traces
for allocation (and deallocation, if relevant) and writes it to the crash dump.

The crash handler runs with elevated privileges so parsing information from a
lesser-privileged process is security sensitive. The GWP-ASan hook is specially
structured to minimize the amount of allocator logic it relies on and to
validate the allocator internals before reasoning about them.

## Status

GWP-ASan is currently only implemented for the system allocator (e.g. not
PartitionAlloc) on Windows. It is not currently enabled by default, but can be
enabled using the following command-line switches (with adjustable parameters):

```shell
chrome --enable-features="GwpAsanMalloc<Study" \
       --force-fieldtrials=Study/Group1 \
       --force-fieldtrial-params=Study.Group1:TotalAllocations/64/AllocationSamplingFrequency/200/ProcessSamplingProbability/1.0
```

## Testing

There is [not yet](https://crbug.com/910751) a way to intentionally trigger a
GWP-ASan exception.

There is [not yet](https://crbug.com/910749) a way to inspect GWP-ASan data in
the minidump (crash report).
