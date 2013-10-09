#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# To run with a custom build of valgrind, e.g., a new release from
# valgrind.org, run this script as
#
#  buildbot/buildbot_valgrind.sh newlib \
#   memcheck_command=/usr/local/google/valgrind/bin/valgrind,--tool=memcheck
#
# NB: memcheck_test is disabled if an alternate memcheck_command is
# used, so remember to temporarily change
# tests/memcheck_test/nacl.scons if you are testing a new valgrind
# that has NaCl patches applied (or when the NaCl patches have been
# upstreamed).

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

if [ $# -lt 1 ]; then
  echo "USAGE: $0 newlib/glibc"
  exit 2
fi

set -x
set -e
set -u

TOOLCHAIN="$1"
shift

if [[ "$TOOLCHAIN" = glibc ]]; then
  GLIBCOPTS="--nacl_glibc"
  SDKHDRINSTALL=""
else
  GLIBCOPTS=""
  SDKHDRINSTALL="install_libpthread"
fi

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out ../xcodebuild ../out \
    src/third_party/nacl_sdk/arm-newlib

echo @@@BUILD_STEP gclient_runhooks@@@
export GYP_DEFINES=target_arch=x64
gclient runhooks --force

echo @@@BUILD_STEP gyp_compile@@@
make -C .. -k -j12 V=1 BUILDTYPE=Debug

echo @@@BUILD_STEP scons_compile@@@
./scons -j 8 -k --verbose ${GLIBCOPTS} --mode=dbg-host,nacl platform=x86-64 "$@"

echo @@@BUILD_STEP memcheck@@@
./scons -k --verbose ${GLIBCOPTS} --mode=dbg-host,nacl platform=x86-64 \
    buildbot=memcheck "$@" memcheck_bot_tests

echo @@@BUILD_STEP leakcheck@@@
./scons -k --verbose ${GLIBCOPTS} --mode=dbg-host,nacl platform=x86-64 \
    buildbot=memcheck run_under_extra_args=--leak-check=full \
    "$@" run_leak_test

# Both the untrusted and trusted tsan tests started failing:
# https://code.google.com/p/nativeclient/issues/detail?id=3396
if [[ "$TOOLCHAIN" != glibc ]]; then

  echo "@@@BUILD_STEP tsan(untrusted)@@@"
  ./scons -k --verbose ${GLIBCOPTS} --mode=dbg-host,nacl platform=x86-64 \
      buildbot=tsan run_under_extra_args= "$@" tsan_bot_tests

  echo "@@@BUILD_STEP tsan(trusted)@@@"
  ./scons -k --verbose ${GLIBCOPTS} --mode=dbg-host,nacl platform=x86-64 \
      buildbot=tsan-trusted run_under_extra_args= "$@" tsan_bot_tests

  echo "@@@BUILD_STEP tsan(trusted, hybrid, RV)@@@"
  # The first RaceVerifier invocation may fail.
  ./scons -k --verbose ${GLIBCOPTS} --mode=dbg-host,nacl platform=x86-64 \
      buildbot=tsan-trusted run_under_extra_args=--hybrid,--log-file=race.log \
      "$@" tsan_bot_tests || true

  echo "== RaceVerifier 2nd run =="

  ./scons -k --verbose ${GLIBCOPTS} --mode=dbg-host,nacl platform=x86-64 \
      buildbot=tsan-trusted run_under_extra_args=--race-verifier=race.log \
      "$@" tsan_bot_tests

fi
