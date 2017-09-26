# Binary Size Metrics

This document lists metrics used to track binary size.

[TOC]

## Metrics for Desktop

 * Sizes are collected by
   [//build/scripts/slave/chromium/sizes.py](https://cs.chromium.org/chromium/build/scripts/slave/chromium/sizes.py)
   * [Win32 Telemetry Graphs](https://chromeperf.appspot.com/report?sid=b3dcc318b51f3780924dfd3d82265ca901ac690cb61af91919997dda9821547c)
   * [Linux Telemetry Graphs](https://chromeperf.appspot.com/report?sid=bd18d34b6d29f26877e7075cb5c34c56c011d99803e9120d61610d7eaef38e9c)
   * [Mac Telemetry Graphs](https://chromeperf.appspot.com/report?sid=2cb6e0a9941e63418e7b83f91583282fa9fbaaafc2d19b3fa1179b28e7d3f7eb)

### Alerting

 * Alerts are sheriffed as part of the main perf sherif rotation.
 * Alerts generally fire for ~100kb jumps.

## Metrics for Android

For Googlers, more information available at [go/chrome-apk-size](https://goto.google.com/chrome-apk-size).

 * Sizes are collected by
   [//build/android/resource_sizes.py](https://cs.chromium.org/chromium/src/build/android/resource_sizes.py).
 * How to analyze Android binary size discussed in [apk_size_regressions.md#debugging-apk-size-increase](../apk_size_regressions.md#debugging-apk-size-increase).
 * Sizes for `ChromePublic.apk`, `ChromeModernPublic.apk`, `MonochromePublic.apk`, `SystemWebview.apk` are tracked.
   * But only `MonochromePublic.apk` is actively monitored.
 * We care most about on-disk size (for users with small device storage)
   * But also care about patch size (so that browser updates get applied)

### Normalized APK Size

 * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=d8f7a27ce61034755ba926dfff9beff7dfbee6f8a596da7f4bb80e3bdd721ad4)
 * Monitored by [Binary Size Sheriffs](../apk_size_regressions.md).
   * Alerts fire for changes of 16kb or more.
 * Computed as:
   * The size of an APK
   * With all native code as if it were stored uncompressed.
   * With all dex code as if it were stored compressed and also extracted.
   * With all translations as if they were not missing (estimates size of missing translations based on size of english strings).
     * Without translation-normalization, translation dumps cause jumps.

### Native Code Size Metrics

 * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=3b4a5766fd4cee11679defe471618fff2fb23bd2f46b901b573c10092f8e03cf)
 * File size of `libchrome.so`
 * Code vs data sections (.text vs .data + .rodata)

### Java Code Size Metrics

 * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=a9fecfc34518cf942ec7841324d05947670c65a0e4e4016f2fe31a66cb76b844)
 * File size of `classes.dex`
 * "Dex": Counts of the number of fields, methods, strings, types.
 * "DexCache": The number of bytes of RAM required to load our dex file (computed from counts)

### Breakdown Metrics

 * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=eeff03cb0872424f121c3adb897e2815eaa891a1261e270e87c052aee770e1f5)
 * APK Size: The file size of `Chrome.apk`
 * Breakdown: The sum of these equals APK Size.

### Install Metrics

 * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=baaba126e7098409c7848647a8cb7c8b101033e3ffdf3420f9d7261cda56a048)
 * Estimated installed size: How much disk is required to install Chrome on a device.
   * This is just an estimate, since actual size depends on Android version (e.g. Dex overhead is very different for pre-L vs L+).
   * Does not include disk used up by profile, etc.
   * We believe it to be fairly accurate for all L+ devices (less space is required for Dalvik runtime pre-L).
 * InstallBreakdown: The sum of these equals Estimated installed size.

### Transfer Size Metrics

 * Deflated apk size:
   * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=f77578280ea9636212c6823b4aa59eb94f785e8de882a25621b59014e36fcf8a)
   * Only relevant for non-patch updates of Chrome (new installs, or manual app updates)
 * Patch Size:
   * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=414b082ff6c4d1841a33dc6b2315e1bfa159f75f85ef93fadf5e4cc46d7ad7da)
   * Uses [https://github.com/googlesamples/apk-patch-size-estimator](https://github.com/googlesamples/apk-patch-size-estimator)
   * No longer runs:
     * Is too slow to be running on the Perf Builder
     * Was found to be fairly unactionable
     * Can be run manually: `build/android/resource_sizes.py --estimate-patch-size out/Release/apks/ChromePublic.apk`

### Uncompressed Metrics

 * [Telemetry Graph](https://chromeperf.appspot.com/report?sid=b91f74582d1069c5491956b6c55fc3071e6645ecdec04de8443633a42acaf0ed)
 * Uncompressed size of classes.dex, locale .pak files, etc
 * Reported only for things that are compressed within the .apk
