# chromium.perf Waterfall

## Overview

The [chromium.perf waterfall](http://build.chromium.org/p/chromium.perf/waterfall)
continuously builds and runs our performance tests on real Android, Windows,
Mac, and Linux hardware. Results are reported to the
[Performance Dashboard](https://chromeperf.appspot.com/) for analysis. The
[Perfbot Health Sheriffing Rotation](perf_bot_sheriffing.md) keeps the build
green and files bugs on regressions. They maintain
[Chrome's Core Principles](https://www.chromium.org/developers/core-principles)
of speed:

> "If you make a change that regresses measured performance, you will be
> required to fix it or revert".

## Contact

  * You can reach the Chromium performance sheriffs at perf-sheriffs@chromium.org.
  * Bugs on waterfall issues should have Component:
    [Speed>Benchmarks>Waterfall](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3ASpeed%3EBenchmarks%3EWaterfall+&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids).

## Links

  * [Perfbot Health Sheriffing Rotation](perf_bot_sheriffing.md)
  * [Debugging an Android Perf Failure](perf_bot_sheriffing.md#Android-Device-failures)
  * TODO: Page on how to repro failures locally
  * TODO: Page on how to connect to bot in lab
  * TODO: Page on how to hack on buildbot/recipe code
  * TODO: Page on bringing up new hardware