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

SCRIPT_DIR="$(dirname "${BASH_SOURCE:-$0}")"

CURRENT_DIR="$(readlink -f "${SCRIPT_DIR}/../../")"
if [[ -z "${CHROME_SRC}" ]]; then
  # If $CHROME_SRC was not set, assume current directory is CHROME_SRC.
  export CHROME_SRC="${CURRENT_DIR}"
fi

if [[ "${CURRENT_DIR/"${CHROME_SRC}"/}" == "${CURRENT_DIR}" ]]; then
  # If current directory is not in $CHROME_SRC, it might be set for other
  # source tree. If $CHROME_SRC was set correctly and we are in the correct
  # directory, "${CURRENT_DIR/"${CHROME_SRC}"/}" will be "".
  # Otherwise, it will equal to "${CURRENT_DIR}"
  echo "Warning: Current directory is out of CHROME_SRC, it may not be \
the one you want."
  echo "${CHROME_SRC}"
fi

# Allow the caller to override a few environment variables. If any of them is
# unset, we default to a sane value that's known to work. This allows for
# experimentation with a custom SDK.
if [[ -z "${ANDROID_NDK_ROOT}" || ! -d "${ANDROID_NDK_ROOT}" ]]; then
  export ANDROID_NDK_ROOT="${CHROME_SRC}/third_party/android_tools/ndk/"
fi
if [[ -z "${ANDROID_SDK_ROOT}" || ! -d "${ANDROID_SDK_ROOT}" ]]; then
  export ANDROID_SDK_ROOT="${CHROME_SRC}/third_party/android_tools/sdk/"
fi

# Add Android SDK tools to system path.
export PATH=$PATH:${ANDROID_SDK_ROOT}/tools
export PATH=$PATH:${ANDROID_SDK_ROOT}/platform-tools

# Add Chromium Android development scripts to system path.
# Must be after CHROME_SRC is set.
export PATH=$PATH:${CHROME_SRC}/build/android

if [[ -n "$CHROME_ANDROID_OFFICIAL_BUILD" ]]; then
  # These defines are used by various chrome build scripts to tag the binary's
  # version string as 'official' in linux builds (e.g. in
  # chrome/trunk/src/chrome/tools/build/version.py).
  export OFFICIAL_BUILD=1
  export CHROMIUM_BUILD="_google_chrome"
  export CHROME_BUILD_TYPE="_official"
fi

android_gyp() {
  echo "Please call build/gyp_chromium instead. android_gyp is going away."
  "${CHROME_SRC}/build/gyp_chromium" --depth="${CHROME_SRC}" --check "$@"
}
