# CPU Profiling Chrome

[TOC]

## Introduction

These are instructions for collecting a CPU profile of chromium. All of the profiling methods described here produce output that can be view using the `pprof` tool. `pprof` is highly customizable; here's a screenshot of some example `pprof` output:

![pprof output screenshot](./media/profile-screenshot.png)

This doc is intended to be an authoritative one-stop resource for profiling chromium. At the time of writing, there are a number of existing docs with profiling instructions, in varying states of obsolescence:

* [./linux/profiling.md](./linux/profiling.md)
* [./profiling_content_shell_on_android.md](./profiling_content_shell_on_android.md)
* https://www.chromium.org/developers/profiling-chromium-and-webkit
* https://www.chromium.org/developers/telemetry/profiling

***promo 
CPU profiling is not to be confused with tracing or task profiling:

* https://www.chromium.org/developers/how-tos/trace-event-profiling-tool
* https://www.chromium.org/developers/threaded-task-tracking
***

## Profiling on Linux

Profiling support is built into tcmalloc and exposed in chromium, so any platform that uses tcmalloc should be able to generate profiling data without using external tools.

### Preparing your checkout

Profiling should always be done on a Release build, which has very similiar performance characteristics to an official build. Make sure the following appears in your `args.gn` file:

    is_debug = false
    enable_profiling = true
    enable_callgrind = true
    blink_symbol_level = 2
    symbol_level = 2
    
### Preparing your environment

By default, the profiler will take a sample 100 times per second. You can adjust this rate by setting the `CPUPROFILE_FREQUENCY` environment variable before launching chromium:

    $ export CPUPROFILE_FREQUENCY=1000
    
The maximum supported rate is 4000 samples per second.

### Profiling a process over its entire lifetime

To profile the main browser process, add the following argument to your chrome invocation:

    --enable-profiling --profiling-at-start

To profile, e.g., every renderer process, add the following argument to your chrome invocation:

    --enable-profiling --profiling-at-start=renderer --no-sandbox

To profile the gpu process, add the following argument to your chrome invocation:

    --enable-profiling --profiling-at-start=gpu-process --no-sandbox --profiling-flush

The gpu process does not shut down cleanly and so requires periodic flushing to
write the profile to disk.

*** promo
The --no-sandbox argument is required to allow the renderer process to write the profiling output to the file system.
***

When the process being profiled ends, you should see one or more `chrome-profile-{process type}-{process ID}` files in your `$PWD`. Run `pprof` to view the results, e.g.:

    $ pprof -web chrome-profile-renderer-12345
    
*** promo
`pprof` is packed with useful features for visualizing profiling data. Try `pprof --help` for more info.
***

*** promo
Tip for Googlers: running `prodaccess` first will make `pprof` run faster, and eliminate some useless spew to the terminal.
***

### Profiling a process or thread for a defined period of time using perf

First, make sure you have the `linux-perf` package installed:

    $ sudo apt-get install linux-perf
    
After starting up the browser and loading the page you want to profile, press 'Shift-Escape' to bring up the task manager, and get the Process ID of the process you want to profile.

Run the perf tool like this:

    $ perf record -g -p <Process ID> -o <output file>
    
*** promo
`perf` does not honor the `CPUPROFILE_FREQUENCY` env var. To adjust the sampling frequency, use the `-F` argument, e.g., `-F 1000`.
***

To stop profiling, press `Control-c` in the terminal window where `perf` is running. Run `pprof` to view the results, providing the path to the browser executable; e.g.:

    $ pprof -web src/out/Release/chrome <perf output file>

*** promo
`pprof` is packed with useful features for visualizing profiling data. Try `pprof --help` for more info.
***

If you want to limit the profile to a single thread, run:

    $ ps -T -p <Process ID> 
    
From the output, find the Thread ID (column header "SPID") of the thread you want. Now run perf:

    $ perf record -g -t <Thread ID> -o <output file>
    
Use the same `pprof` command as above to view the single-thread results.

### Profiling the renderer process for a period defined in javascript

You can generate a highly-focused profile for any period that can be defined in javascript using the `chrome.gpuBenchmarking` javascript interface. First, adding the following command-line flags when you start chrome:

    $ chrome --enable-gpu-benchmarking --no-sandbox [...]

Open devtools, and in the console, use `chrome.gpuBenchmarking.startProfiling` and `chrome.gpuBenchmarking.stopProfiling` to define a profiling period. e.g.:

    > chrome.gpuBenchmarking.startProfiling('perf.data'); doSomething(); chrome.gpuBenchmarking.stopProfiling()

`chrome.gpuBenchmarking` has a number of useful methods for simulating user-gesture-initiated actions; for example, to profile scrolling:

    > chrome.gpuBenchmarking.startProfiling('perf.data'); chrome.gpuBenchmarking.smoothScrollBy(1000, () => { chrome.gpuBenchmarking.stopProfiling() });

### Profiling content_shell with callgrind

This section contains instructions on how to do profiling using the callgrind/cachegrind tools provided by valgrind. This is not a sampling profiler, but a profiler based on running on a simulated CPU. The instructions are Linux-centered, but might work on other platforms too.

#### GN configuration

As with the other options you typically profile a release build with symbols. In order to do so, add enable_profiling to `args.gn`:

```
enable_profiling = true
```

#### Install valgrind

```
sudo apt-get install valgrind
```

#### Profile

Run `content_shell` with callgrind to create a profile. A `callgrind.<pid>` file will be dumped when exiting the browser or stopped with CTRL-C:

```
valgrind --tool=callgrind content_shell --single-process --no-sandbox <url>
```

Alternatively use cachegrind which will give you CPU cycles per code line:

```
valgrind --tool=cachegrind content_shell --single-process --no-sandbox <url>
```

Using single-process is for simple profiling of the renderer. It should be possible to run in multi-process and attach to a renderer process.

#### Install KCachegrind

Warning: this will install a bunch of KDE dependencies.

```
sudo apt-get install kcachegrind
```

#### Explore with KCachegrind

```
kcachegrind callgrind.<pid>
```

## Profiling on Android

Android (Nougat and later) supports profiling using the [simpleperf](https://developer.android.com/ndk/guides/simpleperf) tool.

Follow the [instructions](./android_build_instructions.md) for building and installing chromium on android. With chromium running on the device, run the following command to start profiling on the browser process (assuming your build is in `src/out/Release`):

    $ src/out/Release/bin/chrome_public_apk profile
    Profiler is running; press Enter to stop...
    
Once you stop the profiler, the profiling data will be copied off the device to the host machine and post-processed so it can be viewed in `pprof`, as described above.

To profile the renderer process, you must have just one tab open in chromium, and use a command like this:

    $ src/out/Release/bin/chrome_public_apk profile --profile-process=renderer
    
To limit the profile to a single thread, use a command like this:

    $ src/out/Release/bin/chrome_public_apk profile --profile-process=renderer --profile-thread=main

The `--profile-process` and `--profile-thread` arguments support most of the common process names ('browser', 'gpu', 'renderer') and thread names ('main', 'io', 'compositor', etc.). However, if you need finer control of the process and/or thread to profile, you can specify an explicit Process ID or Thread ID. Check out the usage message for more info:

    $ src/out/Release/bin/chrome_public_apk help profile

## Profiling on ChromeOS

Follow the [simple chrome instructions](../simple_chrome_workflow.md), to build
and deploy chrome to your chromeos device.  These instructions will set up a
build directory for you, so be sure to `gn args out_${SDK_BOARD}/Release` to
edit them and add the gn args listed above.

The easiest way to get a profile is to ssh to your device, which here will
be referred to as `chromeos-box`, but replace that with whatever ip or hostname
your device is.  ssh to your device, create a folder in `/tmp` (which usually
has more space than `/`) and record performance for the entire device.  When
you're done, use scp to copy the perf.data back to your desk and use pprof
as per normal on that perf.data file.

Here's an example:

    $ ssh root@chromeos-box
    localhost ~ # export CPUPROFILE_FREQUENCY=3000
    localhost ~ # mkdir -p /tmp/perf
    localhost ~ # cd /tmp/perf
    localhost /tmp/perf # perf record -g -a -e cycles
    ^C
    [ perf record: Woken up 402 times to write data ]
    [ perf record: Captured and wrote 100.797 MB perf.data (489478 samples) ]
    localhost /tmp/perf # exit
    $ scp root@chromeos-box:/tmp/perf/perf.data .
    $ pprof -web out_${SDK_BOARD}/Release/chrome perf.data

Note: this will complain about missing chromeos symbols.  Even pointing
PPROF\_BINARY\_PATH at the expanded `debug-board.tgz` file that came along with
the chromeos image does not seem to work.  If you can make this work, please
update this doc!

## Profiling during a perf benchmark run

The perf benchmark runner can generate a CPU profile over the course of running a perf test. Currently, this is supported only on Linux and Android. To get info about the relevant options, run:

    $ src/tools/perf/run_benchmark help run
    
... and look for the `--interval-profiling-*` options. For example, to generate a profile of the main thread of the renderer process during the "page interactions" phase of a perf benchmark, you might run:

    $ src/tools/perf/run_benchmark run <benchmark name> --interval-profiling-target=renderer:main --interval-profiling-period=interactions --interval-profiling-frequency=2000

The profiling data will be written into the `artifacts/` sub-directory of your perf benchmark output directory (default is `src/tools/perf`), to files with the naming pattern `*.profile.pb`. You can use `pprof` to view the results, as described above.

## Googlers Only

If you use `pprof -proto chrome-profile-renderer-12345` to turn your perf data
into a proto file, you can then use that resulting file with internal tools.
See [http://go/cprof/user#fs-profiles](http://go/cprof/user#fs-profiles])
for instructions on how to go about this.
