#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Defines functions for envsetup.sh which sets up environment for building
# Chromium on Android.  The build can be either use the Android NDK/SDK or
# android source tree.  Each has a unique init function which calls functions
# prefixed with "common_" that is common for both environment setups.

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

  # The following defines will affect ARM code generation of both C/C++ compiler
  # and V8 mksnapshot.
  case "${TARGET_ARCH}" in
    "arm")
      DEFINES+=" target_arch=arm"
      ;;
    "x86")
      DEFINES+=" target_arch=ia32"
      ;;
    "mips")
      DEFINES+=" target_arch=mipsel"
      ;;
    *)
      echo "TARGET_ARCH: ${TARGET_ARCH} is not supported." >& 2
      print_usage
      return 1
  esac
}


################################################################################
# Prints out help message on usage.
################################################################################
print_usage() {
  echo "usage: ${0##*/} [--target-arch=value] [--help]" >& 2
  echo "--target-arch=value     target CPU architecture (arm=default, x86)" >& 2
  echo "--help                  this help" >& 2
}

################################################################################
# Process command line options.
################################################################################
process_options() {
  while [[ -n $1 ]]; do
    case "$1" in
      --target-arch=*)
        target_arch="$(echo "$1" | sed 's/^[^=]*=//')"
        ;;
      --help)
        print_usage
        return 1
        ;;
      *)
        # Ignore other command line options
        echo "Unknown option: $1"
        ;;
    esac
    shift
  done

  # Sets TARGET_ARCH. Defaults to arm if not specified.
  TARGET_ARCH=${target_arch:-arm}
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

################################################################################
# To build WebView, we use the Android build system and build inside an Android
# source tree.
#############################################################################
webview_build_init() {
  # Use the latest API in the AOSP prebuilts directory (change with AOSP roll).
  android_sdk_version=18

  # For the WebView build we always use the SDK in the Android tree.
  export ANDROID_SDK_ROOT=${ANDROID_BUILD_TOP}/prebuilts/sdk/\
${android_sdk_version}

  common_vars_defines

  # We need to supply SDK paths relative to the top of the Android tree to make
  # sure the generated Android makefiles are portable, as they will be checked
  # into the Android tree.
  ANDROID_SDK=$(python -c \
      "import os.path; print os.path.relpath('${ANDROID_SDK_ROOT}', \
      '${ANDROID_BUILD_TOP}')")
  ANDROID_SDK_TOOLS=$(python -c \
      "import os.path, sys; \
      print os.path.relpath( \
      '${ANDROID_SDK_ROOT}/../tools/' + sys.platform.rstrip('23'), \
      '${ANDROID_BUILD_TOP}')")
  DEFINES+=" android_webview_build=1"
  DEFINES+=" android_src=\$(PWD)"
  DEFINES+=" android_ndk_root=ndk_root_unused_in_webview_build"
  DEFINES+=" android_sdk=\$(PWD)/${ANDROID_SDK}"
  DEFINES+=" android_sdk_root=\$(PWD)/${ANDROID_SDK}"
  DEFINES+=" android_sdk_tools=\$(PWD)/${ANDROID_SDK_TOOLS}"
  DEFINES+=" android_sdk_version=sdk_version_unused_in_webview_build"
  DEFINES+=" android_toolchain=${ANDROID_TOOLCHAIN}"
  if [[ -n "$CHROME_ANDROID_WEBVIEW_OFFICIAL_BUILD" ]]; then
    DEFINES+=" logging_like_official_build=1"
    DEFINES+=" tracing_like_official_build=1"
  fi
  export GYP_DEFINES="${DEFINES}"
}
