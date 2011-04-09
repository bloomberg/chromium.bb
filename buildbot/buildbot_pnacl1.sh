#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

set -x
set -e
set -u


SCONS_COMMON="./scons --mode=opt-linux,nacl --verbose bitcode=1 -j8 -k"


echo @@@BUILD_STEP clobber@@@
rm -rf scons-out toolchain compiler hg ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib

echo @@@BUILD_STEP gclient_runhooks@@@
gclient runhooks --force

echo @@@BUILD_STEP partial_sdk@@@
UTMAN_BUILDBOT=true tools/llvm/utman.sh extrasdk-make-install

for platform in arm x86-32 x86-64 ; do
  echo @@@BUILD_STEP scons smoke_tests [${platform}]@@@
  ${SCONS_COMMON} platform=${platform} smoke_tests

  echo @@@BUILD_STEP scons smoke_tests pic [${platform}]@@@
  ${SCONS_COMMON} platform=${platform} smoke_tests nacl_pic=1

  if [ "${platform}" != "arm" ] ; then
    echo @@@BUILD_STEP scons smoke_tests translator [${platform}]@@@
    ${SCONS_COMMON} platform=${platform} smoke_tests use_sandboxed_translator=1
  fi
done

