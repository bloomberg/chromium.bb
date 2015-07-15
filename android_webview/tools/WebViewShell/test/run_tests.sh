#!/bin/bash

# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PACKAGE_NAME="org.chromium.webview_shell"
DEVICE_WEBVIEW_TEST_PATH="/sdcard/android_webview/tools/WebViewShell/test/"
TESTRUNNER="../../../../build/android/test_runner.py"

$TESTRUNNER instrumentation \
    --test-apk AndroidWebViewShell \
    -f 'WebViewLayoutTest*' \
    --isolate-file-path android_webview/android_webview_shell_test_apk.isolate

if [ "$1" = "rebaseline" ]; then
  adb shell am instrument -w -e mode rebaseline -e class \
      $PACKAGE_NAME.WebViewLayoutTest \
      $PACKAGE_NAME/$PACKAGE_NAME.WebViewLayoutTestRunner
  adb pull $DEVICE_WEBVIEW_TEST_PATH ../test_rebaseline/
fi

exit 0
