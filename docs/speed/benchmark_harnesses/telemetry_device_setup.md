# Setting up devices for Telemetry benchmarks

[TOC]

## Install extra python dependencies

Telemetry depends on pustil version 2.6+. Make sure that you install psutil
first.

If you only use Telemetry through tools/perf/run_benchmark script,
vpython should already automatically install all the required deps for you. e.g:

```
$ tools/perf/run_benchmark --browser=system dummy_benchmark.noisy_benchmark_1
```

## Desktop benchmarks

### Mac, Windows, Linux

We support the most popular version of these OSes (Mac 10.9+, Windows7/8/10,
Linux Ubuntu). In most cases, your desktop environment should be ready to
run Telemetry benchmarks. To keep the benchmark results stable, it’s recommended
that you kill as many background processes on your desktop environment as
possible (e.g: AntiVirus,..) before running the benchmarks.

### ChromeOS

Virtual Machine: see
[cros_vm.md doc](https://chromium.googlesource.com/chromiumos/docs/+/master/cros_vm.md)

Actual CrOS machine: please contact achuith@, cywang@ from CrOS teams for
advice.

## Android benchmarks

To run Telemetry Android benchmarks, you need a host machine and an Android
device attached to the host machine through USB.

**WARNING:** it’s highly recommended that you don’t use your personal Android device
for this. Some of the steps below will wipe out the phone completely.

**Host machine:** we only support Linux Ubuntu as the host.

**Devices:** we only support rooted userdebug devices of Android version K, L,
M, N and O.

### Setting up device ###
*   **Enable USB Debugging:** Go to Settings -> About Phone. Tap many times on
the Build number field. This will enable Developer options. Go to them, enable
USB debugging.
*   For Googler only: follow instructions to
    [flash your device to a userdebug build.](http://go/flash-device)
*   Although not necessary, to make your device behave similar as they do on
    bots, it is recommended to run:
    ```
    export CATAPULT=$CHROMIUM_SRC/third_party/catapult
    $CATAPULT/devil/devil/android/tools/provision_devices.py --disable-network --disable-java-debug
    ```
*   Use one of the supported [`--browser` names](https://github.com/catapult-project/catapult/blob/02ec794ca7c836a14e6d6054e4e337d1ea3acd6f/telemetry/telemetry/internal/backends/chrome/android_browser_finder.py#L30)
    on your
    [run_benchmark](https://cs.chromium.org/chromium/src/tools/perf/run_benchmark)
    or [run_tests](https://cs.chromium.org/chromium/src/tools/perf/run_tests)
    command.
*   Running benchmarks on WebView?
    1.  Add `--remove-system-webview` on the provision devices command above.
    2.  Make sure to [build, install, and enable](https://www.chromium.org/developers/how-tos/build-instructions-android-webview)
        the right WebView apk for your Android build. Also always install
        SystemWebViewShell.apk.
    3. Use `--browser=android-webview` in the `run_benchmark` command.
