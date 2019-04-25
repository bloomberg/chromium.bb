# GWP-ASan

GWP-ASan is a debug tool intended to detect heap memory errors in the wild. It
samples allocations to a debug allocator, similar to ElectricFence or Page Heap,
causing memory errors to crash and report additional debugging context about
the error.

## Allocator

The GuardedPageAllocator returns allocations on pages buffered on both sides by
guard pages. The allocations are either left- or right-aligned to detect buffer
overflows and underflows. When an allocation is freed, the page is marked
inaccessible so use-after-frees cause an exception (until that page is reused
for another allocation.)

The allocator saves stack traces on every allocation and deallocation to
preserve debug context if that allocation results in a memory error.

The allocator implements a quarantine mechanism by allocating virtual memory for
more allocations than the total number of physical pages it can return at any
given time. The difference forms a rudimentary quarantine.

Because pages are re-used for allocations, it's possible that a long-lived
use-after-free will cause a crash long after the original allocation has been
replaced. In order to decrease the likelihood of incorrect stack traces being
reported, we allocate a lot of virtual memory but don't store metadata for every
allocation. That way though we may not be able to report the metadata for an old
allocation, we will not report incorrect stack traces.

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
PartitionAlloc/Oilpan/v8) on Windows and macOS. It is currently enabled by
default. The allocator parameters can be manually modified by using the
following invocation:

```shell
chrome --enable-features="GwpAsanMalloc<Study" \
       --force-fieldtrials=Study/Group1 \
       --force-fieldtrial-params=Study.Group1:MaxAllocations/128/MaxMetadata/255/TotalPages/4096/AllocationSamplingFrequency/1000/ProcessSamplingProbability/1.0
```

GWP-ASan is tuned more aggressively in canary/dev (to increase the likelihood we
catch newly introduced bugs) and for the browser process (because of its
importance and sheer number of allocations.)

A [hotlist of bugs discovered by by GWP-ASan](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=Hotlist%3DGWP-ASan)
exists, though GWP-ASan crashes are filed without external visibility by
default.

## Testing

There is [not yet](https://crbug.com/910751) a way to intentionally trigger a
GWP-ASan exception.

There is [not yet](https://crbug.com/910749) a way to inspect GWP-ASan data in
a minidump (crash report) without access to Google's crash service.
