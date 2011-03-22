#!/bin/bash
# Copyright 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

# Script assumed to be run in native_client/
if [[ ${PWD} != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

set -x
set -e
set -u


echo @@@BUILD_STEP gclient_runhooks@@@
gclient runhooks --force
echo BUILDBOT_GOT_REVISION ${BUILDBOT_GOT_REVISION}
echo BUILDBOT_REVISION ${BUILDBOT_REVISION}

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out tools/SRC tools/BUILD tools/sdk tools/out \
  tools/naclsdk.tgz toolchain || echo already_clean

echo @@@BUILD_STEP compile_toolchain@@@
cd tools
make -j12 buildbot-build-with-glibc TOOLCHAINLOC=sdk

echo @@@BUILD_STEP tar_toolchain@@@
tar cvfz naclsdk.tgz sdk/ && chmod a+r naclsdk.tgz

echo @@@BUILD_STEP untar_toolchain@@@
mkdir -p ../toolchain/linux_x86/.tmp && \
  cd ../toolchain/linux_x86/.tmp && \
  tar xfz ../../../tools/naclsdk.tgz && \
  mv sdk/nacl-sdk/* ../
cd ../../../../

echo @@@BUILD_STEP gyp_compile@@@
make -k -j12 V=1 BUILDTYPE=Release

RETCODE=0

echo @@@BUILD_STEP gyp_tests@@@
cd native_client
python trusted_test.py --config Release || \
  (RETCODE=$? && echo @@@BUILD_FAILED@@@)

echo @@@BUILD_STEP small_tests32@@@
scons -k -j 8 \
  naclsdk_mode=custom:"${PWD}"/toolchain/linux_x86 \
  --mode=dbg-host,nacl platform=x86-32 \
  --nacl_glibc --verbose small_tests || \
  (RETCODE=$? && echo @@@BUILD_FAILED@@@)

echo @@@BUILD_STEP small_tests64@@@
scons -k -j 8 \
  naclsdk_mode=custom:"${PWD}"/toolchain/linux_x86 \
  --mode=dbg-host,nacl platform=x86-64 \
  --nacl_glibc --verbose small_tests || \
  (RETCODE=$? && echo @@@BUILD_FAILED@@@)

# TODO(pasko): add medium_tests, large_tests, {chrome_}browser_tests.

echo @@@BUILD_STEP archive_build@@@
# TODO(pasko): upload the toolchain tarball.

exit ${RETCODE}
