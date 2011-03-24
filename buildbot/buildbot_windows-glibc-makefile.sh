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

export TOOLCHAINLOC=toolchain
export TOOLCHAINNAME=win_x86

echo @@@BUILD_STEP check_glibc_revision_sanity@@@
GLIBC_REVISION="$(tools/glibc_revision.sh)"
if ! curl --fail --location --url http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$GLIBC_REVISION"/glibc_x86.tar.gz > /dev/null; then
  for ((i=1;i<100;i++)); do
    echo "Check if revision "$((REVISION+i))" is available..."
    if curl --fail --location --url http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r"$((GLIBC_REVISION+i))"/glibc_x86.tar.gz > /dev/null; then
      echo @@@BUILD_FAILED@@@
      exit -100
    fi
  done
fi

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out tools/SRC tools/BUILD tools/out tools/toolchain \
  tools/glibc tools/glibc.tar tools/toolchain.tgz toolchain .tmp ||
  echo already_clean
mkdir -p tools/toolchain/win_x86
ln -sfn "$PWD"/cygwin/tmp tools/toolchain/win_x86

echo @@@BUILD_STEP compile_toolchain@@@
(
  cd tools
  make -j8 buildbot-build-with-glibc INST_GLIBC_PROGRAM="$PWD/glibc_download.sh"
  rm toolchain/win_x86/tmp
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

#echo @@@BUILD_STEP gyp_tests@@@
#python trusted_test.py --config Release || \
#  (RETCODE=$? && echo @@@BUILD_FAILED@@@)

#echo @@@BUILD_STEP small_tests32@@@
#./scons -k -j 8 \
#  naclsdk_mode=custom:"${PWD}"/toolchain/win_x86 \
#  --mode=dbg-host,nacl platform=x86-32 \
#  --nacl_glibc --verbose small_tests || \
#  (RETCODE=$? && echo @@@BUILD_FAILED@@@)

#echo @@@BUILD_STEP small_tests64@@@
#./scons -k -j 8 \
#  naclsdk_mode=custom:"${PWD}"/toolchain/win_x86 \
#  --mode=dbg-host,nacl platform=x86-64 \
#  --nacl_glibc --verbose small_tests || \
#  (RETCODE=$? && echo @@@BUILD_FAILED@@@)

# TODO(khim): add small_tests, medium_tests, large_tests, {chrome_}browser_tests.

[[ ${RETCODE} == 0 ]] || exit ${RETCODE}
