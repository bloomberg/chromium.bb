#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Run toolchain torture tests and llvm testsuite tests.
# For now, run on linux64, build and run unsandboxed newlib tests
# for all 3 architectures.

set -o xtrace
set -o nounset
set -o errexit

readonly PNACL_BUILD=pnacl/build.sh
readonly TORTURE_TEST=tools/toolchain_tester/torture_test.sh
readonly LLVM_TESTSUITE=pnacl/scripts/llvm-test-suite.sh
# build.sh, llvm test suite and torture tests all use this value
export PNACL_CONCURRENCY=${PNACL_CONCURRENCY:-4}

export PNACL_BUILDBOT=true

clobber() {
  echo @@@BUILD_STEP clobber@@@
  rm -rf scons-out
  rm -rf toolchain/pnacl*
  # Try to clobber /tmp/ contents to clear temporary chrome files.
  rm -rf /tmp/.org.chromium.Chromium.*
}

handle-error() {
  echo "@@@STEP_FAILURE@@@"
}

#### Support for running arm sbtc tests on this bot, since we have
# less coverage on the main waterfall now:
# http://code.google.com/p/nativeclient/issues/detail?id=2581
readonly SCONS_COMMON="./scons --verbose bitcode=1"
build-sbtc-prerequisites() {
  local platform=$1
  ${SCONS_COMMON} platform=${platform} sel_ldr sel_universal irt_core \
    -j ${PNACL_CONCURRENCY}
}

scons-tests-translator() {
  local platform=$1

  echo "@@@BUILD_STEP scons-tests-translator ${platform}@@@"
  build-sbtc-prerequisites ${platform}
  local use_sbtc="use_sandboxed_translator=1"
  local extra="--mode=opt-host,nacl -j${PNACL_CONCURRENCY} ${use_sbtc} -k"
  ${SCONS_COMMON} ${extra} platform=${platform} "small_tests" || handle-error
  ${SCONS_COMMON} ${extra} platform=${platform} "medium_tests" || handle-error
  ${SCONS_COMMON} ${extra} platform=${platform} "large_tests" || handle-error
}
####

tc-test-bot() {
  clobber

  echo "@@@BUILD_STEP show-config@@@"
  ${PNACL_BUILD} show-config

  # Build the un-sandboxed toolchain
  echo "@@@BUILD_STEP compile_toolchain@@@"
  ${PNACL_BUILD} clean
  ${PNACL_BUILD} all

  # run the torture tests. the "trybot" phases take care of prerequisites
  # for both test sets
  for arch in x8664 x8632 arm; do
    echo "@@@BUILD_STEP torture_tests $arch @@@"
    ${TORTURE_TEST} trybot-pnacl-${arch}-torture \
      --concurrency=${PNACL_CONCURRENCY} || handle-error
  done

  for arch in x86-64 x86-32 arm; do
    echo "@@@BUILD_STEP llvm-test-suite $arch @@@"
    ${LLVM_TESTSUITE} testsuite-prereq ${arch}
    ${LLVM_TESTSUITE} testsuite-clean
    { ${LLVM_TESTSUITE} testsuite-configure ${arch} &&
      ${LLVM_TESTSUITE} testsuite-run ${arch} &&
      ${LLVM_TESTSUITE} testsuite-report ${arch} -v -c
    } || handle-error
  done

  scons-tests-translator "arm"
}

tc-test-bot
