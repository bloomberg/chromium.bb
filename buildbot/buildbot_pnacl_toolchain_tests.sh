#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
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
export PNACL_CONCURRENCY=${PNACL_CONCURRENCY:-8}

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
    if [ $arch = "arm" ]; then
      PNACL_CONCURRENCY=1
    fi
    echo "@@@BUILD_STEP llvm-test-suite $arch @@@"
    ${LLVM_TESTSUITE} testsuite-prereq ${arch}
    ${LLVM_TESTSUITE} testsuite-clean
    { ${LLVM_TESTSUITE} testsuite-configure ${arch} &&
      ${LLVM_TESTSUITE} testsuite-run ${arch} &&
      ${LLVM_TESTSUITE} testsuite-report ${arch} -v
    } || handle-error
  done
}

tc-test-bot
