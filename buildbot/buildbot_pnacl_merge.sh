#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -u
set -e

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo 'ERROR: must be run in native_client/ directory!'
  echo "       (Current directory is $(pwd))"
  exit 1
fi

RETCODE=0

export UTMAN_BUILDBOT=true
export UTMAN_USE_MQ=false
export UTMAN_UPSTREAM=true
export UTMAN_UPSTREAM_SKIP_UPDATE=true

UTMAN=tools/llvm/utman.sh
MERGE_TOOL=tools/llvm/merge-tool.sh
SPEC2K_SCRIPT=buildbot/buildbot_spec2k.sh
UTMAN_TEST=tools/llvm/utman-test.sh

clobber() {
  echo @@@BUILD_STEP clobber@@@
  rm -rf scons-out compiler ../xcodebuild ../sconsbuild ../out \
         src/third_party/nacl_sdk/arm-newlib
  rm -rf toolchain/pnacl* toolchain/hg* toolchain/test-log
  rm -rf ../toolchain
  # Try to clobber /tmp/ contents to clear temporary chrome files.
  rm -rf /tmp/.org.chromium.Chromium.*
}

show-config() {
  echo @@@BUILD_STEP show-config@@@
  ${UTMAN} show-config
}

build-pnacl() {
  echo @@@BUILD_STEP compile_toolchain@@@
  ${UTMAN} clean
  ${UTMAN} everything-translator
}

utman-test() {
  local testname=$1
  echo @@@BUILD_STEP ${testname}@@@
  ${UTMAN_TEST} ${testname} ||
      { RETCODE=$? && echo @@@STEP_FAILURE@@@;}

}

# The slow-bot runs every available test on the last committed merge.
# This includes the scons tests, chrome browser tests, and Spec2K.
# When this bot is all green, we know the merge is in perfect condition,
# and can safely be used in the PNaCl toolchain.
slow-bot() {
  clobber
  show-config
  ${UTMAN} hg-update-upstream
  build-pnacl

  # SCons Tests
  utman-test test-x86-32
  utman-test test-x86-32-pic
  utman-test test-x86-32-browser
  utman-test test-x86-64
  utman-test test-x86-64-pic
  utman-test test-x86-64-browser
  # TODO(pdox): Enable after MC bug is fixed
  # utman-test test-arm
  # utman-test test-arm-pic

  # Spec2K
  CLOBBER=no ${SPEC2K_SCRIPT} pnacl-x8632
  CLOBBER=no ${SPEC2K_SCRIPT} pnacl-x8664
  # TODO(pdox): Enable after MC bug is fixed
  # CLOBBER=no ${SPEC2K_SCRIPT} pnacl-arm
}

# The fast bot performs the actual merges.
#
# Given a new LLVM SVN revision, it performs an automatic merge,
# compiles the toolchain, and then runs some basic tests.
# If those tests pass, then the merge is committed.
#
# This bot must remain 'fast' to keep up with commits on the LLVM tree.
# Otherwise, it becomes difficult to track which upstream revision broke
# the merge. This is why this bot only runs a small sampling of tests.
#
# As a result, imperfect merges may be committed, but those will be
# discovered by the slow-bot.
fast-bot() {
  clobber
  show-config

  # Disable this bot until we are merged
  # up to the tip and all tests are passing
  return 0

  ${MERGE_TOOL} auto
  build-pnacl
  utman-test test-x86-32
  utman-test test-x86-64
  utman-test test-arm
  # TODO(pdox): Add a few Spec2K tests
  commit-and-push-merge
}

commit-and-push-merge() {
  echo "@@@BUILD_STEP commit-and-push-merge@@@"
  if [ ${RETCODE} -eq 0 ]; then
    ${MERGE_TOOL} final-commit
    echo "@@@STEP_SUCCESS@@@"
  else
    echo "Skipping due to prior failure(s)"
    echo "@@@STEP_FAILURE@@@"
  fi
}

if [[ $# -eq 0 ]] ; then
  echo "you must specify a mode on the commandline:"
  exit 1
fi

if [ "$(type -t $1)" != "function" ]; then
  Usage
  echo "ERROR: unknown mode '$1'." >&2
  exit 1
fi

"$@"

if [[ ${RETCODE} != 0 ]]; then
  echo "@@@BUILD_STEP summary@@@"
  echo There were failed stages.
  exit ${RETCODE}
fi
