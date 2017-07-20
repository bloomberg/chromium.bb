#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Sets up environment for building Chromium on Android.

# Make sure we're being sourced (possibly by another script). Check for bash
# since zsh sets $0 when sourcing.
if [[ -n "$BASH_VERSION" && "${BASH_SOURCE:-$0}" == "$0" ]]; then
  echo "ERROR: envsetup must be sourced."
  exit 1
fi

# This only exists to set local variables. Don't call this manually.
android_envsetup_main() {
  local SCRIPT_PATH="$1"
  local SCRIPT_DIR="$(dirname "$SCRIPT_PATH")"
  local CHROME_SRC="$(readlink -f "${SCRIPT_DIR}/../../")"
  local ANDROID_SDK_ROOT="${CHROME_SRC}/third_party/android_tools/sdk/"

  # Add Android SDK and utility tools to the system path.
  export PATH=$PATH:${ANDROID_SDK_ROOT}/platform-tools
  export PATH=$PATH:${ANDROID_SDK_ROOT}/tools/

  # Add Chromium Android development scripts to the system path.
  export PATH=$PATH:${CHROME_SRC}/build/android
}
# In zsh, $0 is the name of the file being sourced.
android_envsetup_main "${BASH_SOURCE:-$0}"
unset -f android_envsetup_main
