# Android Build Instructions

[TOC]

## Prerequisites

A Linux build machine capable of building [Chrome for
Linux](https://chromium.googlesource.com/chromium/src/+/master/docs/linux_build_instructions_prerequisites.md).
Other (Mac/Windows) platforms are not supported for Android.

## Getting the code

First, check out and install the [depot\_tools
package](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up).

Then, if you have no existing checkout, create your source directory and
get the code:

```shell
mkdir ~/chromium && cd ~/chromium
fetch --nohooks android # This will take 30 minutes on a fast connection
```

If you have an existing Linux checkout, you can add Android support by
appending `target_os = ['android']` to your .gclient file (in the
directory above src):

```shell
cat > .gclient <<EOF
 solutions = [ ...existing stuff in here... ]
 target_os = [ 'android' ] # Add this to get Android stuff checked out.
EOF
```

Then run gclient sync to get the Android stuff checked out:

```shell
gclient sync
```

## (Optional) Check out LKGR

If you want a single build of Chromium in a known good state, sync to
the LKGR ("last known good revision"). You can find it
[here](http://chromium-status.appspot.com/lkgr), and the last 100
[here](http://chromium-status.appspot.com/revisions). Run:

```shell
gclient sync --nohooks -r <lkgr-sha1>
```

This is not needed for a typical developer workflow; only for one-time
builds of Chromium.

## Configure your build

Android builds can be run with GN or GYP, though GN incremental builds
are the fastest option and GN will soon be the only supported option.
They are both meta-build systems that generate nina files for the
Android build. Both builds are regularly tested on the build waterfall.

### Configure GYP (deprecated -- use GN instead)

If you are using GYP, next to the .gclient file, create a a file called
'chromium.gyp_env' with the following contents:

```shell
echo "{ 'GYP_DEFINES': 'OS=android target_arch=arm', }" > chromium.gyp_env
```

Note that "arm" is the default architecture and can be omitted. If
building for x86 or MIPS devices, change `target_arch` to "ia32" or
"mipsel".

 **NOTE:** If you are using the `GYP_DEFINES` environment variable, it
will override any settings in this file. Either clear it or set it to
the values above before running `gclient runhooks`.

 See
[build/android/developer\_recommended\_flags.gypi](https://code.google.com/p/chromium/codesearch#chromium/src/build/android/developer_recommended_flags.gypi&sq=package:chromium&type=cs&q=file:android/developer_recommended_flags.gypi&l=1)
for other recommended GYP settings.
 Once chromium.gyp_env is ready, you need to run the following command
to update projects from gyp files. You may need to run this again when
you have added new files, updated gyp files, or sync'ed your
repository.

```shell
gclient runhooks
```

#### This will download more things and prompt you to accept Terms of Service for Android SDK packages.

## Configure GN (recommended)

If you are using GN, create a build directory and set the build flags
with:

```shell
gn args out/Default
```

 You can replace out/Default with another name you choose inside the out
directory. Do not use GYP's out/Debug or out/Release directories, as
they may conflict with GYP builds.

Also be aware that some scripts (e.g. tombstones.py, adb_gdb.py)
require you to set `CHROMIUM_OUTPUT_DIR=out/Default`.

This command will bring up your editor with the GN build args. In this
file add:

```
target_os = "android"
target_cpu = "arm"  # (default)
is_debug = true  # (default)

# Other args you may want to set:
is_component_build = true
is_clang = true
symbol_level = 1  # Faster build with fewer symbols. -g1 rather than -g2
enable_incremental_javac = true  # Much faster; experimental
symbol_level = 1 # Faster build with fewer symbols. -g1 rather than -g2
enable_incremental_javac = true # Much faster; experimental
```

You can also specify `target_cpu` values of "x86" and "mipsel". Re-run
gn args on that directory to edit the flags in the future. See the [GN
build
configuration](https://www.chromium.org/developers/gn-build-configuration)
page for other flags you may want to set.

### Install build dependencies

Update the system packages required to build by running:

```shell
./build/install-build-deps-android.sh
```

Make also sure that OpenJDK 1.7 is selected as default:

`sudo update-alternatives --config javac`
`sudo update-alternatives --config java`
`sudo update-alternatives --config javaws`
`sudo update-alternatives --config javap`
`sudo update-alternatives --config jar`
`sudo update-alternatives --config jarsigner`

### Synchronize sub-directories.

```shell
gclient sync
```

## Build and install the APKs

If the `adb_install_apk.py` script below fails, make sure aapt is in
your PATH. If not, add aapt's path to your PATH environment variable (it
should be
`/path/to/src/third_party/android_tools/sdk/build-tools/{latest_version}/`).

Prepare the environment:

```shell
. build/android/envsetup.sh
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
