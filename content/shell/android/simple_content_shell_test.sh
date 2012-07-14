#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Simple test launches a content shell to load a URL, and waits 5 seconds before
# checking there are two content shell processes (grep ps for 'content_shell').
# If multiple devices are connected, the test runs on the first one listed by
# adb devices. The script expects your environment to already be set up.

if [ $# -ne 1 ]; then
  echo "Error: Please specify content shell location"
  exit 1
fi

CONTENT_SHELL_APK=$1
DEV=$(adb devices | grep device | grep -v devices | awk '{ print $1 }' \
        | head -n 1)

if [[ -z $DEV ]]; then
  echo "Error: No connected devices. Device needed to run content shell test."
  exit 1
fi

# Reinstall content shell.  This will also kill existing content_shell procs.
adb -s ${DEV} uninstall org.chromium.content_shell

if [ ! -f "${CONTENT_SHELL_APK}" ]; then
  echo "Error: Could not find specified content shell apk to install."
  exit 1
fi
adb -s ${DEV} install -r ${CONTENT_SHELL_APK}

# Launch a content shell process to open about:version
adb -s ${DEV} shell am start -n \
  org.chromium.content_shell/.ContentShellActivity -d "about:version"

# Wait 5 seconds, to give the content shell some time to crash.
sleep 5

# Get the number of content shell procs that exist.
NUM_PROCS=$(adb -s ${DEV} shell ps | grep content_shell | wc | awk '{print $1}')

# Check that there are two content shell processes (browser and renderer).
if [[ "${NUM_PROCS}" -lt 2 ]]; then
  echo "ERROR: Expected two content shell processes (browser and renderer)."
  echo "       Found ${NUM_PROCS} 'content_shell' process(es)."
  # Uninstall content shell to clean up.
  adb -s ${DEV} uninstall org.chromium.content_shell
  exit 1
fi

# Uninstall content shell to clean up.
adb -s ${DEV} uninstall org.chromium.content_shell
