#!/bin/bash
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Bash functions used by buildbot annotator scripts for the android
# build of chromium.  Executing this script should not perform actions
# other than setting variables and defining of functions.

# Number of jobs on the compile line; e.g.  make -j"${JOBS}"
JOBS="${JOBS:-4}"

# Clobber build?  Overridden by bots with BUILDBOT_CLOBBER.
NEED_CLOBBER="${NEED_CLOBBER:-0}"

# Setup environment for Android build.
# Called from bb_baseline_setup.
# Moved to top of file so it is easier to find.
function bb_setup_environment {
  export ANDROID_SDK_ROOT=/usr/local/google/android-sdk-linux
  export ANDROID_NDK_ROOT=/usr/local/google/android-ndk-r7
}

# Install the build deps by running
# build/install-build-deps-android-sdk.sh.  This may update local tools.
# $1: source root.
function bb_install_build_deps {
  echo "@@@BUILD_STEP install build deps android@@@"
  local script="$1/build/install-build-deps-android-sdk.sh"
  if [[ -f "$script" ]]; then
    "$script"
  else
    echo "Cannot find $script; why?"
  fi
}

# Function to force-green a bot.
function bb_force_bot_green_and_exit {
  echo "@@@BUILD_STEP Bot forced green.@@@"
  exit 0
}

# Basic setup for all bots to run after a source tree checkout.
# $1: source root.
function bb_baseline_setup {
  echo "@@@BUILD_STEP cd into source root@@@"
  SRC_ROOT="$1"
  if [ ! -d "${SRC_ROOT}" ] ; then
    echo "Please specify a valid source root directory as an arg"
    echo '@@@STEP_FAILURE@@@'
    return 1
  fi
  cd $SRC_ROOT

  if [ ! -f build/android/envsetup.sh ] ; then
    echo "No envsetup.sh"
    echo "@@@STEP_FAILURE@@@"
    return 1
  fi

  echo "@@@BUILD_STEP Basic setup@@@"
  bb_setup_environment

  for mandatory_directory in $(dirname "${ANDROID_SDK_ROOT}") \
    $(dirname "${ANDROID_NDK_ROOT}") ; do
    if [[ ! -d "${mandatory_directory}" ]]; then
      echo "Directory ${mandatory_directory} does not exist."
      echo "Build cannot continue."
      echo "@@@STEP_FAILURE@@@"
      return 1
    fi
  done

  if [ ! "$BUILDBOT_CLOBBER" = "" ]; then
    NEED_CLOBBER=1
  fi

  # Setting up a new bot?  Must do this before envsetup.sh
  if [ ! -d "${ANDROID_NDK_ROOT}" ] ; then
    bb_install_build_deps $1
  fi

  echo "@@@BUILD_STEP Configure with envsetup.sh@@@"
  . build/android/envsetup.sh

  if [ "$NEED_CLOBBER" -eq 1 ]; then
    echo "@@@BUILD_STEP Clobber@@@"
    rm -rf "${SRC_ROOT}"/out
    if [ -e "${SRC_ROOT}"/out ] ; then
      echo "Clobber appeared to fail?  ${SRC_ROOT}/out still exists."
      echo "@@@STEP_WARNINGS@@@"
    fi
  fi

  echo "@@@BUILD_STEP android_gyp@@@"
  android_gyp
}


# Setup goma.  Used internally to buildbot_functions.sh.
function bb_setup_goma_internal {

  # Quick bail if I messed things up and can't wait for the CQ to
  # flush out.
  # TODO(jrg): remove this condition when things are
  # proven stable (4/1/12 or so).
  if [ -f /usr/local/google/DISABLE_GOMA ]; then
    echo "@@@STEP_WARNINGS@@@"
    echo "Goma disabled with a local file"
    return
  fi

  goma_dir=${goma_dir:-/b/build/goma}
  if [ -f ${goma_dir}/goma.key ]; then
    export GOMA_API_KEY_FILE=${GOMA_DIR}/goma.key
  fi
  local goma_ctl=$(which goma_ctl.sh)
  if [ "${goma_ctl}" != "" ]; then
    local goma_dir=$(dirname ${goma_ctl})
  fi

  if [ ! -f ${goma_dir}/goma_ctl.sh ]; then
    echo "@@@STEP_WARNINGS@@@"
    echo "Goma not found on this machine; defaulting to make"
    return
  fi
  export GOMA_DIR=${goma_dir}
  echo "GOMA_DIR: " $GOMA_DIR

  export GOMA_COMPILER_PROXY_DAEMON_MODE=true
  export GOMA_COMPILER_PROXY_RPC_TIMEOUT_SECS=300
  export PATH=$GOMA_DIR:$PATH

  echo "Starting goma"
  if [ "$NEED_CLOBBER" -eq 1 ]; then
    ${GOMA_DIR}/goma_ctl.sh restart
  else
    ${GOMA_DIR}/goma_ctl.sh ensure_start
  fi
  trap bb_stop_goma_internal SIGHUP SIGINT SIGTERM
}

# Stop goma.
function bb_stop_goma_internal {
  echo "Stopping goma"
  ${GOMA_DIR}/goma_ctl.sh stop
}

# $@: make args.
# Use goma if possible; degrades to non-Goma if needed.
function bb_goma_make {
  bb_setup_goma_internal

  if [ "${GOMA_DIR}" = "" ]; then
    make -j${JOBS} "$@"
    return
  fi

  HOST_CC=$GOMA_DIR/gcc
  HOST_CXX=$GOMA_DIR/g++
  TARGET_CC=$(/bin/ls $ANDROID_TOOLCHAIN/*-gcc | head -n1)
  TARGET_CXX=$(/bin/ls $ANDROID_TOOLCHAIN/*-g++ | head -n1)
  TARGET_CC="$GOMA_DIR/gomacc $TARGET_CC"
  TARGET_CXX="$GOMA_DIR/gomacc $TARGET_CXX"
  COMMON_JAVAC="$GOMA_DIR/gomacc /usr/bin/javac -J-Xmx512M \
    -target 1.5 -Xmaxerrs 9999999"

  command make \
    -j100 \
    -l20 \
    HOST_CC="$HOST_CC" \
    HOST_CXX="$HOST_CXX" \
    TARGET_CC="$TARGET_CC" \
    TARGET_CXX="$TARGET_CXX" \
    CC.host="$HOST_CC" \
    CXX.host="$HOST_CXX" \
    CC.target="$TARGET_CC" \
    CXX.target="$TARGET_CXX" \
    LINK.target="$TARGET_CXX" \
    COMMON_JAVAC="$COMMON_JAVAC" \
    "$@"

  bb_stop_goma_internal
}

# Compile step
function bb_compile {
  # This must be named 'compile', not 'Compile', for CQ interaction.
  # Talk to maruel for details.
  echo "@@@BUILD_STEP compile@@@"
  bb_goma_make
}

# Re-gyp and compile with unit test bundles configured as shlibs for
# the native test runner.  Experimental for now.  Once the native test
# loader is on by default, this entire function becomes obsolete.
function bb_compile_apk_tests {
  echo "@@@BUILD_STEP Re-gyp for the native test runner@@@"
  GYP_DEFINES="$GYP_DEFINES gtest_target_type=shared_library" android_gyp

  echo "@@@BUILD_STEP Native test runner compile@@@"
  bb_goma_make
}

# Experimental compile step; does not turn the tree red if it fails.
function bb_compile_experimental {
  # Linking DumpRenderTree appears to hang forever?
  EXPERIMENTAL_TARGETS="android_experimental"
  for target in ${EXPERIMENTAL_TARGETS} ; do
    echo "@@@BUILD_STEP Experimental Compile $target @@@"
    set +e
    bb_goma_make -k "${target}"
    if [ $? -ne 0 ] ; then
      echo "@@@STEP_WARNINGS@@@"
    fi
    set -e
  done
}

# Run tests on an emulator.
function bb_run_tests_emulator {
  echo "@@@BUILD_STEP Run Tests on an Emulator@@@"
  build/android/run_tests.py -e --xvfb --verbose
}

# Run tests on an actual device.  (Better have one plugged in!)
function bb_run_tests {
  echo "@@@BUILD_STEP Run Tests on actual hardware@@@"
  build/android/run_tests.py --xvfb --verbose
}

# Run APK tests on an actual device.
function bb_run_apk_tests {
  echo "@@@BUILD_STEP Run APK Tests on actual hardware@@@"
  tempfile=/tmp/tempfile-$$.txt
  # Filter out STEP_FAILURES, we don't want REDNESS on test failures for now.
  build/android/run_tests.py --xvfb --verbose --apk=True \
    | sed 's/@@@STEP_FAILURE@@@//g' | tee $tempfile
  happy_failure=$(cat $tempfile | grep RUNNER_FAILED | wc -l)
  if [[ $happy_failure -eq 0 ]] ; then
    echo "@@@STEP_WARNINGS@@@"
  fi
  rm -f $tempfile
}
