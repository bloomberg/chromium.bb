#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script assumed to be run in native_client/
if [[ ${PWD} != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

set -x
set -e
set -u

export TOOLCHAINLOC=toolchain
export TOOLCHAINNAME=linux_x86

echo @@@BUILD_STEP gclient_runhooks@@@
gclient runhooks --force

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out tools/SRC tools/BUILD tools/out tools/toolchain \
  tools/glibc tools/glibc.tar tools/toolchain.tgz toolchain .tmp ||
  echo already_clean

echo @@@BUILD_STEP compile_toolchain@@@
(
  cd tools
  make -j8 buildbot-build-with-glibc
  make install-glibc INST_GLIBC_PREFIX="$PWD"
)

echo @@@BUILD_STEP tar_toolchain@@@
(
  cd tools
  tar cSvfz toolchain.tgz toolchain/ && chmod a+r toolchain.tgz
)

echo @@@BUILD_STEP tar_glibc@@@
(
  cd tools/glibc
  tar cSvfz ../glibc.tgz * && chmod a+r ../glibc.tgz
)

echo @@@BUILD_STEP untar_toolchain@@@
(
  mkdir -p .tmp
  cd .tmp
  tar zxf ../tools/toolchain.tgz
  mv toolchain ..
)

echo @@@BUILD_STEP gyp_compile@@@
(
  cd ..
  make -k -j8 V=1 BUILDTYPE=Release
)

RETCODE=0

echo @@@BUILD_STEP gyp_tests@@@
python trusted_test.py --config Release || \
  (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP small_tests32@@@
./scons -k -j 8 \
  naclsdk_mode=custom:"${PWD}"/toolchain/linux_x86 \
  --mode=dbg-host,nacl platform=x86-32 \
  --nacl_glibc --verbose small_tests || \
  (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP small_tests64@@@
./scons -k -j 8 \
  naclsdk_mode=custom:"${PWD}"/toolchain/linux_x86 \
  --mode=dbg-host,nacl platform=x86-64 \
  --nacl_glibc --verbose small_tests || \
  (RETCODE=$? && echo @@@STEP_FAILURE@@@)

# TODO(pasko): add medium_tests, large_tests, {chrome_}browser_tests.

[[ ${RETCODE} == 0 ]] || exit ${RETCODE}

echo @@@BUILD_STEP archive_build@@@
/b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
  tools/toolchain.tgz \
  gs://nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/toolchain_linux_x86.tar.gz
echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/@@@

echo @@@BUILD_STEP archive_glibc@@@
wget http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$(tools/glibc_revision.sh)"/glibc_x86.tar.gz -O /dev/null ||
/b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
  tools/glibc.tgz \
  gs://nativeclient-archive2/between_builders/x86_glibc/r"$(tools/glibc_revision.sh)"/glibc_x86.tar.gz
echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$(tools/glibc_revision.sh)"/@@@
