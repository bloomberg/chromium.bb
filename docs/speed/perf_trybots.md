# Perf Try Bots

[TOC]

## What are perf try bots?

Chrome has a performance lab with dozens of device and OS configurations. You
can run performance tests on an unsubmitted CL on these devices using the
perf try bots.

## Supported platforms

The platforms available in the lab change over time. To find the currently
available platforms, run `tools/perf/run_benchmark try --help`.

Example output:

```
> tools/perf/run_benchmark try --help
usage: Run telemetry benchmarks on trybot. You can add all the benchmark options available except the --browser option
       [-h] [--repo_path <repo path>] [--deps_revision <deps revision>]
       <trybot name> <benchmark name>

positional arguments:
  <trybot name>         specify which bots to run telemetry benchmarks on.  Allowed values are:
                        Mac Builder
                        all
                        all-android
                        all-linux
                        all-mac
                        all-win
                        android-fyi
                        android-nexus5
                        android-nexus5X
                        android-nexus6
                        android-nexus7
                        android-one
                        android-webview-arm64-aosp
                        android-webview-nexus6-aosp
                        linux
                        mac-10-11
                        mac-10-12
                        mac-10-12-mini-8gb
                        mac-air
                        mac-pro
                        mac-retina
                        staging-android-nexus5X
                        staging-linux
                        staging-mac-10-12
                        staging-win
                        win
                        win-8
                        win-x64
                        winx64-10
                        winx64-high-dpi
                        winx64-zen
                        winx64ati
                        winx64intel
                        winx64nvidia

```

## Supported benchmarks

All the telemetry benchmarks are supported by the perf trybots. To get a full
list, run `tools/perf/run_benchmark list`.

To learn more about the benchmark, you can read about the
[system health benchmarks](https://docs.google.com/document/d/1BM_6lBrPzpMNMtcyi2NFKGIzmzIQ1oH3OlNG27kDGNU/edit?ts=57e92782),
which test Chrome's performance at a high level, and the
[benchmark harnesses](https://docs.google.com/spreadsheets/d/1ZdQ9OHqEjF5v8dqNjd7lGUjJnK6sgi8MiqO7eZVMgD0/edit#gid=0),
which cover more specific areas.

## Starting a perf try job

Use this command line:

`tools/perf/run_benchmark try <trybot_name> <benchmark_name>`

See above for how to choose a trybot and benchmark.

Run `tools/perf/run_benchmark try --help` for more information about available
options.

## Interpreting the results

Perf trybots create a code review under the covers to hold the trybot results.
The code review will list links to buildbot status pages for the try jobs.
On each buildbot status page, you will see a "HTML Results" link. You can click
it to see detailed information about the performance test results with and
without your patch.

**[Here is the documentation](https://github.com/catapult-project/catapult/blob/master/docs/metrics-results-ui.md)**
on reading the results.