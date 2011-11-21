#!/bin/bash
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# "compile and run tests" script for the android build of chromium.
# Intended for use by buildbot.
# At this time, we only have one bot which is both a builder and
# tester.  Script assumes it runs in the "build" directory.
#
# This script uses buildbot "Annotator" style for steps.
# This script does not sync the source tree.

set -e
set -x

NEED_CLOBBER=0

echo "@@@BUILD_STEP cd into source root@@@"
SRC_ROOT=$(cd "$(dirname $0)/../.."; pwd)
cd $SRC_ROOT

echo "@@@BUILD_STEP Basic setup@@@"
export ANDROID_SDK_ROOT=/usr/local/google/android-sdk-linux_x86
export ANDROID_NDK_ROOT=/usr/local/google/android-ndk-r7
for mandatory_directory in "${ANDROID_SDK_ROOT}" "${ANDROID_NDK_ROOT}" ; do
  if [[ ! -d "${mandatory_directory}" ]]; then
    echo "Directory ${mandatory_directory} does not exist."
    echo "Build cannot continue."
    exit 1
  fi
done

if [ ! -n "$BUILDBOT_CLOBBER" ]; then
  NEED_CLOBBER=1
fi

## Build and test steps

echo "@@@BUILD_STEP Configure with envsetup.sh@@@"
. build/android/envsetup.sh

if [ "$NEED_CLOBBER" -eq 1 ]; then
  echo "@@@BUILD_STEP Clobber@@@"
  rm -rf "${SRC_ROOT}"/out
fi

echo "@@@BUILD_STEP android_gyp@@@"
android_gyp

echo "@@@BUILD_STEP Compile@@@"
make -j4

echo "@@@BUILD_STEP Run Tests@@@"
build/android/run_tests.py -e --xvfb

exit 0
