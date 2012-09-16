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

RETCODE=0
TOOLCHAIN_LABEL="pnacl_linux_x86"
GSBASE="gs://nativeclient-archive2/pnacl/between_bots/llvm"

export PNACL_BUILDBOT=true

GSUTIL=buildbot/gsutil.sh
PNACL_BUILD=pnacl/build.sh
MERGE_TOOL=pnacl/scripts/merge-tool-git.sh
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

  echo "@@@BUILD_STEP show-config@@@"
  ${PNACL_BUILD} show-config

  # If BUILDBOT_REVISION is blank, use LLVM tip of tree
  if [ -z "${BUILDBOT_REVISION}" ]; then
    # TODO(dschuff): find out what vars are set on the LLVM bot
    ${MERGE_TOOL} pull-upstream
    export BUILDBOT_REVISION=$(${MERGE_TOOL} get-head-revision)
  fi

  if [ -z "${BUILDBOT_BUILDNUMBER:-}" ]; then
    echo "Please set BUILDBOT_BUILDNUMBER"
    exit 1
  fi

  local ret=0
  # Sync the rest of pnacl sources before doing the merge, because it
  # steps on the LLVM source
  ${PNACL_BUILD} sync-sources

  # Merge LLVM
  ${MERGE_TOOL} clean
  ${MERGE_TOOL} auto ${BUILDBOT_REVISION} || ret=$?
  if [ ${ret} -eq 55 ] ; then
    echo '@@@BUILD_STEP No changes, skipping tests@@@'
    exit 0
  elif [ ${ret} -ne 0 ]; then
    echo '@@@STEP_FAILURE@@@'
    exit 1
  fi


  # Variables for build.sh
  export PNACL_PRUNE=true

  # Build the un-sandboxed toolchain
  echo "@@@BUILD_STEP compile_toolchain@@@"
  ${PNACL_BUILD} clean
  # Don't sync-sources here
  ${PNACL_BUILD} build-all
  ${PNACL_BUILD} translator-all

  #echo "@@@BUILD_STEP archive_toolchain@@@"
  #${PNACL_BUILD} tarball pnaclsdk.tgz
  #${GSUTIL} cp pnaclsdk.tgz \
  #  ${GSBASE}/${BUILDBOT_BUILDNUMBER}/pnaclsdk.tgz

  # For now, run all these on a single bot, since the waterfall
  # won't behave correctly otherwise.
  scons-bot
  spec2k-x86-bot
  spec2k-arm-bot
}

download-toolchain() {
  echo "@@@BUILD_STEP download_toolchain@@@"
  if [ -z "${BUILDBOT_TRIGGERED_BY_BUILDNUMBER:-}" ]; then
    echo "Please set BUILDBOT_TRIGGERED_BY_BUILDNUMBER"
    exit 1
  fi
  ${GSUTIL} cp ${GSBASE}/${BUILDBOT_TRIGGERED_BY_BUILDNUMBER}/pnaclsdk.tgz \
               pnaclsdk.tgz
  rm -rf toolchain/pnacl*
  mkdir -p toolchain/${TOOLCHAIN_LABEL}
  cd toolchain/${TOOLCHAIN_LABEL}
  tar -zxvf ../../pnaclsdk.tgz
  cd ../..
}

scons-bot() {
  #download-toolchain
  local concurrency=8
  FAIL_FAST=false ${PNACL_SCRIPT} test-all-newlib ${concurrency} || RETCODE=$?
}

spec2k-x86-bot() {
  #download-toolchain
  CLOBBER=no ${SPEC2K_SCRIPT} pnacl-x8632 || RETCODE=$?
  CLOBBER=no ${SPEC2K_SCRIPT} pnacl-x8664 || RETCODE=$?
}

spec2k-arm-bot() {
  #download-toolchain
  CLOBBER=no ${SPEC2K_SCRIPT} pnacl-arm || RETCODE=$?
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

if [[ ${RETCODE} -ne 0 ]] ; then
  echo "@@@BUILD_STEP summary@@@"
  echo "There were failed stages."
fi

exit ${RETCODE}
