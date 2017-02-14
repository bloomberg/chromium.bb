# Checking out and building Chromium for Android

There are instructions for other platforms linked from the
[get the code](get_the_code.md) page.

## Instructions for Google Employees

Are you a Google employee? See
[go/building-chrome](https://goto.google.com/building-chrome) instead.

[TOC]

## System requirements

* A 64-bit Intel machine running Linux with at least 8GB of RAM. More
  than 16GB is highly recommended.
* At least 100GB of free disk space.
* You must have Git and Python installed already.

Most development is done on Ubuntu. Other distros may or may not work;
see the [Linux instructions](linux_build_instructions.md) for some suggestions.

Building the Android client on Windows or Mac is not supported and doesn't work.

## Install `depot_tools`

Clone the `depot_tools` repository:

```shell
$ git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git
```

Add `depot_tools` to the end of your PATH (you will probably want to put this
in your `~/.bashrc` or `~/.zshrc`). Assuming you cloned `depot_tools`
to `/path/to/depot_tools`:

```shell
$ export PATH="$PATH:/path/to/depot_tools"
```

## Get the code

Create a `chromium` directory for the checkout and change to it (you can call
this whatever you like and put it wherever you like, as
long as the full path has no spaces):

```shell
$ mkdir ~/chromium && cd ~/chromium
$ fetch --nohooks android
```

If you don't want the full repo history, you can save a lot of time by
adding the `--no-history` flag to `fetch`.

Expect the command to take 30 minutes on even a fast connection, and many
hours on slower ones.

If you've already installed the build dependencies on the machine (from another
checkout, for example), you can omit the `--nohooks` flag and `fetch`
will automatically execute `gclient runhooks` at the end.

When `fetch` completes, it will have created a hidden `.gclient` file and a
directory called `src` in the working directory. The remaining instructions
assume you have switched to the `src` directory:

```shell
$ cd src
```

### Converting an existing Linux checkout

If you have an existing Linux checkout, you can add Android support by
appending `target_os = ['android']` to your `.gclient` file (in the
directory above `src`):

```shell
$ echo "target_os = [ 'android' ]" >> ../.gclient
```

Then run `gclient sync` to pull the new Android dependencies:

```shell
$ gclient sync
```

(This is the only difference between `fetch android` and `fetch chromium`.)

### Install additional build dependencies

Once you have checked out the code, run

```shell
$ build/install-build-deps-android.sh
```

to get all of the dependencies you need to build on Linux, *plus* all of the
Android-specific dependencies (you need some of the regular Linux dependencies
because an Android build includes a bunch of the Linux tools and utilities).

### Run the hooks

Once you've run `install-build-deps` at least once, you can now run the
Chromium-specific hooks, which will download additional binaries and other
things you might need:

```shell
$ gclient runhooks
```

*Optional*: You can also [install API
keys](https://www.chromium.org/developers/how-tos/api-keys) if you want your
build to talk to some Google services, but this is not necessary for most
development and testing purposes.

### Configure the JDK

Make also sure that OpenJDK 1.7 is selected as default:

```shell
$ sudo update-alternatives --config javac
$ sudo update-alternatives --config java
$ sudo update-alternatives --config javaws
$ sudo update-alternatives --config javap
$ sudo update-alternatives --config jar
$ sudo update-alternatives --config jarsigner
```

## Setting up the build

Chromium uses [Ninja](https://ninja-build.org) as its main build tool along
with a tool called [GN](../tools/gn/docs/quick_start.md) to generate `.ninja`
files. You can create any number of *build directories* with different
configurations. To create a build directory which builds Chrome for Android,
run:

```shell
$ gn gen --args='target_os="android"' out/Default
```

* You only have to run this once for each new build directory, Ninja will
  update the build files as needed.
* You can replace `Default` with another name, but
  it should be a subdirectory of `out`.
* For other build arguments, including release settings, see [GN build
  configuration](https://www.chromium.org/developers/gn-build-configuration).
  The default will be a debug component build matching the current host
  operating system and CPU.
* For more info on GN, run `gn help` on the command line or read the
  [quick start guide](../tools/gn/docs/quick_start.md).

Also be aware that some scripts (e.g. `tombstones.py`, `adb_gdb.py`)
require you to set `CHROMIUM_OUTPUT_DIR=out/Default`.

## Build Chromium

Build Chromium with Ninja using the command:

```shell
$ ninja -C out/Default chrome_public_apk
```

You can get a list of all of the other build targets from GN by running `gn ls
out/Default` from the command line. To compile one, pass the GN label to Ninja
with no preceding "//" (so, for `//chrome/test:unit_tests` use `ninja -C
out/Default chrome/test:unit_tests`).

## Installing and Running Chromium on a device

If the `adb_install_apk.py` script below fails, make sure `aapt` is in your
PATH. If not, add `aapt`'s parent directory to your `PATH` environment variable
(it should be
`/path/to/src/third_party/android_tools/sdk/build-tools/{latest_version}/`).

Prepare the environment:

```shell
$ . build/android/envsetup.sh
```

### Plug in your Android device

Make sure your Android device is plugged in via USB, and USB Debugging
is enabled.

To enable USB Debugging:

*   Navigate to Settings \> About Phone \> Build number
*   Click 'Build number' 7 times
*   Now navigate back to Settings \> Developer Options
*   Enable 'USB Debugging' and follow the prompts

You may also be prompted to allow access to your PC once your device is
plugged in.

You can check if the device is connected by running:

```shell
third_party/android_tools/sdk/platform-tools/adb devices
```

Which prints a list of connected devices. If not connected, try
unplugging and reattaching your device.

### Build the full browser

```shell
ninja -C out/Release chrome_public_apk
```

And deploy it to your Android device:

```shell
CHROMIUM_OUTPUT_DIR=$gndir build/android/adb_install_apk.py $gndir/apks/ChromePublic.apk # for gn.
```

The app will appear on the device as "Chromium".

### Build Content shell

Wraps the content module (but not the /chrome embedder). See
[https://www.chromium.org/developers/content-module](https://www.chromium.org/developers/content-module)
for details on the content module and content shell.

```shell
ninja -C out/Release content_shell_apk
build/android/adb_install_apk.py out/Release/apks/ContentShell.apk
```

this will build and install an Android apk under
`out/Release/apks/ContentShell.apk`. (Where `Release` is the name of your build
directory.)

If you use custom out dir instead of standard out/ dir, use
CHROMIUM_OUT_DIR env.

```shell
export CHROMIUM_OUT_DIR=out_android
```

### Build WebView shell

[Android WebView](https://developer.android.com/reference/android/webkit/WebView.html)
is a system framework component. Since Android KitKat, it is implemented using
Chromium code (based off the [content module](https://dev.chromium.org/developers/content-module)).
It is possible to test modifications to WebView using a simple test shell. The
WebView shell is a view with a URL bar at the top (see [code](https://code.google.com/p/chromium/codesearch#chromium/src/android_webview/test/shell/src/org/chromium/android_webview/test/AwTestContainerView.java))
and is **independent** of the WebView **implementation in the Android system** (
the WebView shell is essentially a standalone unbundled app).
As drawback, the shell runs in non-production rendering mode only.

```shell
ninja -C out/Release android_webview_apk
build/android/adb_install_apk.py out/Release/apks/AndroidWebView.apk
```

If, instead, you want to build the complete Android WebView framework component and test the effect of your chromium changes in other Android app using the WebView, you should follow the [Android AOSP + chromium WebView instructions](https://www.chromium.org/developers/how-tos/build-instructions-android-webview)

### Running

Set [command line flags](https://www.chromium.org/developers/how-tos/run-chromium-with-flags) if necessary.

For Content shell:

```shell
build/android/adb_run_content_shell  http://example.com
```

For Chrome public:

```shell
build/android/adb_run_chrome_public  http://example.com
```

For Android WebView shell:

```shell
build/android/adb_run_android_webview_shell http://example.com
```

### Logging and debugging

Logging is often the easiest way to understand code flow. In C++ you can print
log statements using the LOG macro or printf(). In Java, you can print log
statements using [android.util.Log](https://developer.android.com/reference/android/util/Log.html):

`Log.d("sometag", "Reticulating splines progress = " + progress);`

You can see these log statements using adb logcat:

```shell
adb logcat...01-14 11:08:53.373 22693 23070 D sometag: Reticulating splines progress = 0.99
```

You can debug Java or C++ code. To debug C++ code, use one of the
following commands:

```shell
build/android/adb_gdb_content_shell
build/android/adb_gdb_chrome_public
build/android/adb_gdb_android_webview_shell http://example.com
```

See [Debugging Chromium on Android](https://www.chromium.org/developers/how-tos/debugging-on-android)
for more on debugging, including how to debug Java code.

### Testing

For information on running tests, see [android\_test\_instructions.md](https://chromium.googlesource.com/chromium/src/+/master/docs/android_test_instructions.md).

### Faster Edit/Deploy (GN only)

GN's "incremental install" uses reflection and side-loading to speed up the edit
& deploy cycle (normally < 10 seconds). The initial launch of the apk will be
a little slower since updated dex files are installed manually.

*   Make sure to set` is_component_build = true `in your GN args
*   All apk targets have \*`_incremental` targets defined (e.g.
    `chrome_public_apk_incremental`) except for Webview and Monochrome

Here's an example:

```shell
ninja -C out/Default chrome_public_apk_incremental
out/Default/bin/install_chrome_public_apk_incremental -v
```

For gunit tests (note that run_*_incremental automatically add
--fast-local-dev when calling test\_runner.py):

```shell
ninja -C out/Default base_unittests_incremental
out/Default/bin/run_base_unittests_incremental
```

For instrumentation tests:

```shell
ninja -C out/Default chrome_public_test_apk_incremental
out/Default/bin/run_chrome_public_test_apk_incremental
```

To uninstall:

```shell
out/Default/bin/install_chrome_public_apk_incremental -v --uninstall
```

A subtly erroneous flow arises when you build a regular apk but install an
incremental apk (e.g.
`ninja -C out/Default foo_apk && out/Default/bin/install_foo_apk_incremental`).
Setting `incremental_apk_by_default = true` in your GN args aliases regular
targets as their incremental counterparts. With this arg set, the commands
above become:

```shell
ninja -C out/Default chrome_public_apk
out/Default/bin/install_chrome_public_apk

ninja -C out/Default base_unittests
out/Default/bin/run_base_unittests

ninja -C out/Default chrome_public_test_apk
out/Default/bin/run_chrome_public_test_apk
```

If you want to build a non-incremental apk you'll need to remove
`incremental_apk_by_default` from your GN args.

## Tips, tricks, and troubleshooting

### Rebuilding libchrome.so for a particular release

These instructions are only necessary for Chrome 51 and earlier.

In the case where you want to modify the native code for an existing
release of Chrome for Android (v25+) you can do the following steps.
Note that in order to get your changes into the official release, you'll
need to send your change for a codereview using the regular process for
committing code to chromium.

1.  Open Chrome on your Android device and visit chrome://version
2.  Copy down the id listed next to "Build ID:"
3.  Go to
    [http://storage.googleapis.com/chrome-browser-components/BUILD\_ID\_FROM\_STEP\_2/index.html](http://storage.googleapis.com/chrome-browser-components/BUILD_ID_FROM_STEP_2/index.html)
4.  Download the listed files and follow the steps in the README.
