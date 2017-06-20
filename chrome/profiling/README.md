# chrome/profiling

This directory contains the code for the "profiling" process. This is in
active development and is not ready for use.

Design doc: https://docs.google.com/document/d/1eRAgOFgHwYEPge8G1_5UEvu8TJs5VkYCxd6aFU8AIKY

Currently this is used for out-of-process logging of heap profiling data and
is enabled by setting the GN flag `enable_oop_heap_profiling`. The in-process
code that communicates with the profiling process is in
`//chrome/common/profiling`.
