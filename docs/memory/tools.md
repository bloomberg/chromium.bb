# Description of Tools for developers trying to understand memory usage

This page provides an overview of the tools available for examining memory usage
in chrome.

## Which tool should I use?

No single tool can give a full view of memory usage in Chrome. There are too
many different context involved (JS heap, DOM objects, native allocations, GPU,
etc) that any tool that collected all that information likely would not be able
to provide an actionable analysis.

Here is a table of common area of inquiry and suggested tools for examining them.

| Topic/Area of Inquiry  | Tool(s) | What this is good for | Caveats |
|----------|------------| ----------
| Object allocation over time | [`diff_heap_profiler.py`](#diff-heap-profiler) | Finding leaked C++ objects over time. |
| Self-reported stats for resource usage per subsystem  | [memory-infra in Chrome Tracing](#tracing-memory-infra) | Gaining a global overview of which subsystems are using memory. |
| Suspected Renderer DOM leaks | [Real World Leak Detector](#real-world-leak-detector) | Finding leaks within the Renderer where C++ objects are refcounted using Oilpan |
| Kernel/Drive Resource Usage | [perfmon (win), ETW](#os-tools) | Finding resource leaks that are not normally considered when the term "memory" is used. |
| Blackbox examination of process memory | [VMMAP (win)](#os-tools) | Understanding fragmentation of the memory space |

If that seems like a lot of tools and complexity, it is [but there's a reason](#no-one-true-metric).

## <a name="diff-heap-profiler"></a> `diff_heap_profiler.py`
### Instructions
  1. Ensure browser was started with OOPHP enabled and that the right processes
     are being profiled. TODO(awong): link instructions.
  2. Visit chrome://memory-internals and click "Save Dump" for a baseline.
  3. Do work that triggers events/behaviors to measure.
  2. Visit chrome://memory-internals and click "Save Dump" for a the next
     sample.
  4. Symbolize both dump files by running [`symbolize_trace.py`](../../third_party/catapult/experimental/tracing/bin/symbolize_trace.py)
  4. Run resulting traces through [`diff_heap_profiler.py`](../../third_party/catapult/experimental/tracing/bin/diff_heap_profiler.py) to show a list of new
     allocations.

### What this is good for
Examining allocations that occur within one point in time. This is often useful
for finding leaks as one call-stack will rise to the top as the leak is repeated
triggered.

Multiple traces can be given at once to show incremental changes.

TODO(awong): Write about options to script and the flame graph.

### Blindspots
  * Only catches allocations that pass through the allocator shim


## <a name="tracing-memory-infra"></a> Chrome tracing and Memory-infra

## <a name="real-world-leak-detector"></a> Real World Leak Detector (Blink-only)

## <a name="os-tools"></a> OS Tools: perfmon, ETW, VMMAP

## <a name="no-one-true-metric"></a> No really, I want one tool/metric that views everything. Can I has it plz?
Sorry. No.

There is a natrual tradeoff between getting detailed information
and getting reliably complete information. Getting detailed information requires
instrumentation which adds complexity and selection bias to the measurement.
This reduces the reliability and completeness of the metric as code shifts over
time.

While it might be possible to instrument a specific Chrome heap
(eg, PartitionAlloc or Oilpan, or even shimming malloc()) to gather detailed
actionable data, this implicitly means the instrumentation code is making
assumptions about what process resources are used which may not be complete
or correct.

As an example of missed coverage, none of these collection methods
can notice kernel resources that are allocated (eg, GPU memory, or drive memory
such as the Windows Paged and Non-paged pools) as side effects of user mode
calls nor do they account for memory that does not go through new/malloc
(manulaly callling `mmap()`, or `VirtualAlloc()`). Querying a full view of
these allocaitons usually requires admin privileges, the semantics change
per platform, and the performance can vary from being "constant-ish" to
being dependent on virtual space size (eg, probing allocation via
VirtualQueryEx or parsing /proc/self/maps) or number of proccesses in the
system (NTQuerySystemInformation).

As an example of error in measurement, PartitionAlloc did not account for
the Windows Committed Memory model [bug](https://crbug.com/765406) leading to
a "commit leak" in Windows that was undetected in its self-reported stats.

Relying on a single metric or single tool will thus either selection bias
the data being read or not give enough detail to quickly act on problems.
