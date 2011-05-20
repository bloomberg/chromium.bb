#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

if [ $# -ne 1 ]; then
  echo "USAGE: $0 newlib/glibc"
  exit 2
fi

set -x
set -e
set -u

TOOLCHAIN="$1"

if [[ "$TOOLCHAIN" = glibc ]]; then
  GLIBCOPTS="--nacl_glibc"
  SDKHDRINSTALL=""
else
  GLIBCOPTS=""
  SDKHDRINSTALL="install_libpthread"
fi

echo @@@BUILD_STEP gclient_runhooks@@@
export GYP_DEFINES=target_arch=x64
gclient runhooks --force

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out toolchain compiler hg ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib

echo @@@BUILD_STEP partial_sdk@@@
./scons --verbose --mode=nacl_extra_sdk platform=x86-64 --download \
${GLIBCOPTS} extra_sdk_update_header ${SDKHDRINSTALL} extra_sdk_update

echo @@@BUILD_STEP gyp_compile@@@
make -C .. -k -j12 V=1 BUILDTYPE=Debug

echo @@@BUILD_STEP gyp_tests@@@
python trusted_test.py --config Debug --bits 64

echo @@@BUILD_STEP scons_compile@@@
./scons -j 8 DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=dbg-linux,nacl,doc platform=x86-64

if [[ "$TOOLCHAIN" != glibc ]]; then

  echo @@@BUILD_STEP memcheck@@@
  ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
      ${GLIBCOPTS} --mode=dbg-linux,nacl platform=x86-64 sdl=none \
      buildbot=memcheck memcheck_bot_tests

  echo @@@BUILD_STEP leakcheck@@@
  ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
      ${GLIBCOPTS} --mode=dbg-linux,nacl platform=x86-64 sdl=none \
      buildbot=memcheck run_under_extra_args=--leak-check=full run_leak_test

  echo "@@@BUILD_STEP tsan(untrusted)@@@"
  ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
      ${GLIBCOPTS} --mode=dbg-linux,nacl platform=x86-64 sdl=none \
      buildbot=tsan run_under_extra_args= tsan_bot_tests

  echo "@@@BUILD_STEP tsan(trusted)@@@"
  ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
      ${GLIBCOPTS} --mode=dbg-linux,nacl platform=x86-64 sdl=none \
      buildbot=tsan-trusted run_under_extra_args= tsan_bot_tests

  echo "@@@BUILD_STEP tsan(trusted, hybrid, RV)@@@"
  # The first RaceVerifier invocation may fail.
  ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
      ${GLIBCOPTS} --mode=dbg-linux,nacl platform=x86-64 sdl=none \
      buildbot=tsan-trusted run_under_extra_args=--hybrid,--log-file=race.log \
      tsan_bot_tests || true

  echo "== RaceVerifier 2nd run =="

  ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
      ${GLIBCOPTS} --mode=dbg-linux,nacl platform=x86-64 sdl=none \
      buildbot=tsan-trusted run_under_extra_args=--race-verifier=race.log \
      tsan_bot_tests

fi
