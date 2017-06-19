# Memory

Landing page for all things related to memory usage in Chromium.

The goal is to keep an ever present set of links and references that will
help people what is actively happening in the memory space. Please keep
this landing page short and action oriented.

That being said, please also send CL with update and changes. This should
reflect current active status, and it's easier to do that if everyone helps
maintain it. :)

## How is chrome's memory usage doing in the world?

Look at UMA for the Memory.\* UMAs. Confused at which to use? Start with these:


| name | description | Caveats |
|------|-------------|---------|
| Memory.\*.Committed | private, image, and file mapped memory | Windows only. Over penalizes Chrome for mapped images and files |
| Memory.Experimental.\*.<br />PrivateMemoryFootprint | New metric measuring private anonymous memory usage (swap or ram) by Chrome. | See Consistent Memory  Metrics |
| --Memory.\*.Large2-- | Measures physical memory usage. | **DO NOT USE THIS METRIC**\* |

\*Do **NOT** use `Memory.\*.Large2` as the `Large2` metrics only
count the physical ram used. This means the number varies based on the behavior
of applications other than Chrome making it near meaningless. Yes, they are
currently in the default finch trials. We're going to fix that.


## How do developers communicate?

Note, these channels are for developer coordination and NOT user support. If
you are a Chromium user experiencing a memory related problem, file a bug
instead.

| name | description |
|------|-------------|
| [memory-dev@chromium.org]() | Discussion group for all things memory related. Post docs, discuss bugs, etc., here. |
| chrome-memory@google.com | Google internal version of the above. Use sparingly. |
| https://chromiumdev.slack.com/messages/memory/ | Slack channel for real-time discussion with memory devs. Lots of C++ sadness too. |
| crbug [Performance=Memory](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Performance%3DMemory) label | Bucket with auto-filed and user-filed bugs. |
| crbug [Stability=Memory](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=Stability%3DMemory) label | Tracks mostly OOM crashes. |


## I have a reproducible memory problem, what do I do?

Yay! Please file a [memory
bug](https://bugs.chromium.org/p/chromium/issues/entry?template=Memory%20usage).

If you are willing to do a bit more, please grab a memory infra trace and upload
that. Here are [instructions for MacOS](https://docs.google.com/document/d/15mBOu_uZbgP5bpdHZJXEnF9csSRq7phUWXnZcteVr0o/edit).
(TODO: Add instructions for easily grabbing a trace for all platforms.)


## I'm a dev and I want to help. How do I get started?

Great! First, sign up for the mailing lists above and check out the slack channel.

Second, familiarize yourself with the following:

| Topic | Description |
|-------|-------------|
| [Key Concepts in Chrome Memory](/memory/key_concepts.md) | Primer for memory terminology in Chrome. |
| [memory-infra](/memory-infra/README.md) | The primary tool used for inspecting allocations. |


## What are people actively working on?
| Project | Description |
|---------|-------------|
| [Memory Coordinator](https://docs.google.com/document/d/1dkUXXmpJk7xBUeQM-olBpTHJ2MXamDgY_kjNrl9JXMs/edit#heading=h.swke19b7apg5) (including [Purge+Throttle/Suspend](https://docs.google.com/document/d/1EgLimgxWK5DGhptnNVbEGSvVn6Q609ZJaBkLjEPRJvI/edit)) | Centralized policy and coordination of all memory components in Chrome |
| [Memory-Infra](/memory-infra/README.md) | Tooling and infrastructure for Memory |
| [System health benchmarks](https://docs.google.com/document/d/1pEeCnkbtrbsK3uuPA-ftbg4kzM4Bk7a2A9rhRYklmF8/edit?usp=sharing) | Automated tests based on telemetry |


## Key knowledge areas and contacts
| Knowledge Area | Contact points |
|----------------|----------------|
| Chrome on Android | mariahkomenko, dskiba, ssid |
| Browser Process | mariahkomenko, dskiba, ssid |
| GPU/cc | ericrk |
| Memory metrics | erikchen, primano, ajwong, wez |
| Native Heap Profiling | primiano, dskiba, ajwong |
| Net Stack | mmenke, rsleevi, xunjieli |
| Renderer Process | haraken, tasak, hajimehoshi, keishi, hiroshige |
| V8 | hpayer, ulan, verwaest, mlippautz |


## Other docs
  * [Why we work on memory](https://docs.google.com/document/d/1jhERqimO-LtuplzQzbBv1vK7SVOh63AMf2irJI2LOqU/edit)
  * [TOK/LON memory-dev@ meeting notes](https://docs.google.com/document/d/1tCTw9lnjs85t8GFiiyae2hbu6lrz8kysFCgMCKUvcXo/edit)
  * [Memory convergence 3 (in Mountain View, 2017 May)](https://docs.google.com/document/d/1FBIqBGIa0DSaFsh-QjmVvoC82pGuOgiQDIhc8-vzXbQ/edit)
  * [TRIM convergence 2 (in Mountain View, 2016 Nov)](https://docs.google.com/document/d/17Kef7UxjR6VW_ehVbsc-DI0IU7TQk-2C56JSbzbPuhA/edit)
  * [TRIM convergence 1 (in Munich, 2016 Apr)](https://docs.google.com/document/d/1PGcM6iVBp0OYh3m8xGQhOgkQK0obQy8YWwoefP9NZCA/edit#)

