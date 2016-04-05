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
