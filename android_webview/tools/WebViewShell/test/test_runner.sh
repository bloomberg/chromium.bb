#!/bin/bash

# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if ! which adb &> /dev/null; then
  echo "adb is not in your path, did you run envsetup.sh?"
  exit -1
fi

REBASELINE="false"
UPDATE_LAYOUT_TESTS_FROM_BLINK="true"
DEVICE_WEBVIEW_TEST_FILES_PATH="/data/local/tmp/webview_test/"
DEVICE_WEBVIEW_REBASELINE_PATH="/data/data/org.chromium.webview_shell/files/"
PACKAGE_NAME="org.chromium.webview_shell"

BLINK_LAYOUT_TEST_BASE="../../../../third_party/WebKit/LayoutTests/"
WEBVIEW_TEST_BASE="../../../android_webview/tools/WebViewShell/test/"

BLINK_LAYOUT_FILES_TO_COPY="
webexposed/resources/global-interface-listing.js
webexposed/global-interface-listing.html"

if [ "$UPDATE_LAYOUT_TESTS_FROM_BLINK" == "true" ]; then
  echo "copying layout test files from $BLINK_LAYOUT_TEST_BASE"
  cd $BLINK_LAYOUT_TEST_BASE
  for f in $BLINK_LAYOUT_FILES_TO_COPY
  do
    echo $BLINK_LAYOUT_TEST_BASE$f
    cp --parents $f $WEBVIEW_TEST_BASE
  done
  cd -
  pwd
fi

adb push . $DEVICE_WEBVIEW_TEST_FILES_PATH

if [ "$REBASELINE" != "true" ]; then
  adb shell am instrument -w -e class $PACKAGE_NAME.WebViewLayoutTest \
      $PACKAGE_NAME/$PACKAGE_NAME.WebViewLayoutTestRunner
else
  adb shell am instrument -w -e mode rebaseline -e class \
      $PACKAGE_NAME.WebViewLayoutTest \
      $PACKAGE_NAME/$PACKAGE_NAME.WebViewLayoutTestRunner
  adb pull $DEVICE_WEBVIEW_REBASELINE_PATH ../test_rebaseline/
  adb shell rm -rf $DEVICE_WEBVIEW_REBASELINE_PATH
fi

adb shell rm -rf $DEVICE_WEBVIEW_TEST_FILES_PATH

exit 0
