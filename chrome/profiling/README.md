# chrome/profiling

This document describes the architecture for the profiling process, which
tracks memory allocations in other processes. See [design doc] for more details.

[design doc]: https://docs.google.com/document/d/1eRAgOFgHwYEPge8G1_5UEvu8TJs5VkYCxd6aFU8AIKY

There is some additional information in //chrome/common/profiling/README.md

How To Enable Out of Process Heap Profiling
-------------------------------------------
Navigate to `chrome://flags/#memlog` and set the flag to
"Profile only the browser process." It's possible to profile all processes, but
that has a higher performance impact, and heap dumps of renderer processes are
less actionable.

How To Use Out of Process Heap Profiling
-------------------------------------------
By default, you don't need to do anything. The heap profiler will detect when
the browser's memory usage has exceeded a certain threshold and upload a trace.

To force an upload, or to create a heap dump manually, see
`chrome://memory-internals`. The resulting heap dump is intended to be used with
[symbolize_trace][1] and [diff_heap_profiler.py][2].

It's also possible to view the heap dump within a [memory-infra][3] trace,
although the results will still need to be symbolized with [symbolize_trace][1].

Due to size constraints, most allocations are pruned from the heap dump. Only
allocations of sufficient size and/or frequency are kept. After pruning, the
result is ~100x smaller, but still accounts for about half of all allocations.
More importantly, it still accounts for all obvious memory leaks.

[1]: https://cs.chromium.org/chromium/src/third_party/catapult/tracing/bin/symbolize_trace
[2]: https://cs.chromium.org/chromium/src/third_party/catapult/experimental/tracing/bin/diff_heap_profiler.py
[3]: /docs/memory-infra/README.md

Communication Model
-------------------
When profiling is enabled, the browser process will spawn the profiling service.
The services lives in a sandboxed, utility process, and its interface is at
`chrome/common/profiling/memlog.mojom`.

All other processes, including the browser process, are MemlogClients. See
`memlog_client.mojom`. Depending on the profiling mode, the browser process will
start profiling for just itself [`--memlog=browser`] , or itself and all child
processes [`--memlog=all`].

The browser process creates a pair of pipes for each MemlogClient. The sending
pipe is sent to the MemlogClient, and the receiving pipe is sent to the
profiling service. Each MemlogClient then sends memory allocation information
via its endpoint to the profiling service.

Code Locations
--------------
`//chrome/common/profiling` - Logic for MemlogClient.
`//chrome/browser/profiling_host` - Logic in browser process for starting
profiling service, and connecting MemlogClients to the profiling service.
`//chrome/profiling` - Profiling service.

