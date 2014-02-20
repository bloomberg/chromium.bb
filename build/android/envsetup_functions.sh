#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Defines functions for envsetup.sh which sets up environment for building
# Chromium for Android using the Android NDK/SDK.

################################################################################
# Exports environment variables common to both sdk and non-sdk build (e.g. PATH)
# based on CHROME_SRC, along with DEFINES for GYP_DEFINES.
################################################################################
common_vars_defines() {
  # Add Android SDK tools to system path.
  export PATH=$PATH:${ANDROID_SDK_ROOT}/tools
  export PATH=$PATH:${ANDROID_SDK_ROOT}/platform-tools

  # Add Chromium Android development scripts to system path.
  # Must be after CHROME_SRC is set.
  export PATH=$PATH:${CHROME_SRC}/build/android

  # The set of GYP_DEFINES to pass to gyp.
  DEFINES="OS=android"

  if [[ -n "$CHROME_ANDROID_OFFICIAL_BUILD" ]]; then
    # These defines are used by various chrome build scripts to tag the binary's
    # version string as 'official' in linux builds (e.g. in
    # chrome/trunk/src/chrome/tools/build/version.py).
    export OFFICIAL_BUILD=1
    export CHROMIUM_BUILD="_google_chrome"
    export CHROME_BUILD_TYPE="_official"
  fi

  # TODO(thakis), Jan 18 2014: Remove this after two weeks or so, after telling
  # everyone to set use_goma in GYP_DEFINES instead of a GOMA_DIR env var.
  if [[ -d $GOMA_DIR ]]; then
    DEFINES+=" use_goma=1 gomadir=$GOMA_DIR"
  fi
}


################################################################################
# Process command line options.
################################################################################
process_options() {
  while [[ -n $1 ]]; do
    case "$1" in
      --target-arch=*)
        echo "ERROR: --target-arch is ignored."
        echo "Pass -Dtarget_arch=foo to gyp instead."
        echo "(x86 is spelled ia32 in gyp, mips becomes mipsel, arm stays arm)"
        return 1
        ;;
      *)
        # Ignore other command line options
        echo "Unknown option: $1"
        ;;
    esac
    shift
  done
}

################################################################################
# Initializes environment variables for NDK/SDK build.
################################################################################
sdk_build_init() {
  # Allow the caller to override a few environment variables. If any of them is
  # unset, we default to a sane value that's known to work. This allows for
  # experimentation with a custom SDK.
  if [[ -z "${ANDROID_NDK_ROOT}" || ! -d "${ANDROID_NDK_ROOT}" ]]; then
    export ANDROID_NDK_ROOT="${CHROME_SRC}/third_party/android_tools/ndk/"
  fi
  if [[ -z "${ANDROID_SDK_ROOT}" || ! -d "${ANDROID_SDK_ROOT}" ]]; then
    export ANDROID_SDK_ROOT="${CHROME_SRC}/third_party/android_tools/sdk/"
  fi

  common_vars_defines

  export GYP_DEFINES="${DEFINES}"

  if [[ -n "$CHROME_ANDROID_BUILD_WEBVIEW" ]]; then
    # Can not build WebView with NDK/SDK because it needs the Android build
    # system and build inside an Android source tree.
    echo "Can not build WebView with NDK/SDK.  Requires android source tree." \
        >& 2
    echo "Try . build/android/envsetup.sh instead." >& 2
    return 1
  fi
}
