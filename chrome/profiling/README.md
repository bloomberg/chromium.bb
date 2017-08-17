# chrome/profiling

This document describes the architecture for the profiling process, which
tracks memory allocations in other processes.

Design doc: https://docs.google.com/document/d/1eRAgOFgHwYEPge8G1_5UEvu8TJs5VkYCxd6aFU8AIKY

How To Enable Out of Process Heap Profiling
-------------------------------------------
Compile with the GN flag `enable_oop_heap_profiling = true`. On Windows, you
must also build a static, release binary.

To turn it on, pass the command line flag `--memlog=browser` to profile just the
browser process, or `--memlog=all` to profile all processes. Navigate to
`chrome://memory-internals`, and press the "Dump" button to trigger a memory
dump for a given process. This creates a dump called `memlog_dump.json.gz` in
the profile directory.

The `--memlog` flag is also exposed in chrome://flags/#memlog


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

