#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -u
set -e
set -x

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo 'ERROR: must be run in native_client/ directory!'
  echo "       (Current directory is $(pwd))"
  exit 1
fi

echo "****************** Environment ******************"
env

export UTMAN_BUILDBOT=true

UTMAN=tools/llvm/utman.sh
MERGE_TOOL=tools/llvm/merge-tool.sh
SPEC2K_SCRIPT=buildbot/buildbot_spec2k.sh
PNACL_SCRIPT=buildbot/buildbot_pnacl.sh

clobber() {
  echo @@@BUILD_STEP clobber@@@
  rm -rf scons-out
  rm -rf toolchain/pnacl*
  # Try to clobber /tmp/ contents to clear temporary chrome files.
  rm -rf /tmp/.org.chromium.Chromium.*
}

# merge-bot:
# Merge with the given SVN revision, and run every test.
# This includes the scons tests, chrome browser tests, and Spec2K.
# When this bot is all green, we know the merge is in perfect condition,
# and can safely be used in the PNaCl toolchain.
merge-bot() {
  clobber
  # Sanity check for BUILDBOT_REVISION
  if [ ${BUILDBOT_REVISION} -lt 139000 ]; then
     echo 'ERROR: BUILDBOT_REVISION is invalid'
     exit 1
  fi

  echo '@@@BUILD_STEP Merge@@@'
  local ret=0
  ${MERGE_TOOL} clean
  ${MERGE_TOOL} auto ${BUILDBOT_REVISION} || ret=$?
  if [ ${ret} -eq 55 ] ; then
    echo '@@@BUILD_STEP No changes, skipping tests@@@'
    exit 0
  elif [ ${ret} -ne 0 ]; then
    echo '@@@STEP_FAILURE@@@'
    exit 1
  fi

  echo "@@@BUILD_STEP show-config@@@"
  ${UTMAN} show-config

  echo "@@@BUILD_STEP compile_toolchain@@@"
  ${UTMAN} clean
  LLVM_PROJECT_REV=${BUILDBOT_REVISION} \
  UTMAN_MERGE_TESTING=true \
    ${UTMAN} everything-translator

  # SCons Tests
  local concurrency=8
  FAIL_FAST=false ${PNACL_SCRIPT} mode-test-all ${concurrency}

  # Spec2K
  CLOBBER=no ${SPEC2K_SCRIPT} pnacl-arm
  CLOBBER=no ${SPEC2K_SCRIPT} pnacl-x8632
  CLOBBER=no ${SPEC2K_SCRIPT} pnacl-x8664
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
