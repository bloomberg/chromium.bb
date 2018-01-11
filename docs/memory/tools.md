# Description of Tools for developers trying to understand memory usage

This page provides an overview of the tools available for examining memory usage
in chrome.

# Which tool should I use?

No single tool can give a full view of memory usage in Chrome. There are too
many different context involved (JS heap, DOM objects, native allocations, GPU,
etc) that any tool that collected all that information likely would not be able
to provide an actionable analysis. As such, here is a table of common area of
inquiry and suggested tools for examining them.

| Topic/Area of Inquiry  | Tool(s) | What this is good for | Caveats |
|----------|------------| ----------
| Object allocation over time | `diff_heap_profiler.py` | Finding leaked C++ objects over time. |
| Self-reported stats for resource usage per subsystem  | chrome tracing with memory-infra | Gaining a global overview of which subsystems are using memory. |
| Suspected Renderer DOM leaks | Real World Leak Detector | Finding leaks within the Renderer where C++ objects are refcounted using Oilpan |
| Kernel/Drive Resource Usage | perfmon (win), ETW | Finding resource leaks that are not normally considered when the term "memory" is used. |
| Blackbox examination of process memory | VMMAP (win) | Understanding fragmentation of the memory space |
