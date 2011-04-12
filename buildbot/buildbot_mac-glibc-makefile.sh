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
export TOOLCHAINNAME=mac_x86
export INST_GLIBC_PROGRAM="$PWD/tools/glibc_download.sh"

echo @@@BUILD_STEP gclient_runhooks@@@
gclient runhooks --force

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out tools/SRC tools/BUILD tools/out tools/toolchain \
  tools/glibc tools/glibc.tar tools/toolchain.tgz toolchain .tmp ||
  echo already_clean
mkdir -p tools/toolchain/mac_x86

# glibc_download.sh can return three return codes:
#  0 - glibc is successfully downloaded and installed
#  1 - glibc is not downloaded but another run may help
#  2+ - glibc is not downloaded and can not be downloaded later
#
# If the error result is 2 or more we are stopping the build
echo @@@BUILD_STEP check_glibc_revision_sanity@@@
echo "Try to download glibc revision $(tools/glibc_revision.sh)"
if tools/glibc_download.sh tools/toolchain/mac_x86 1; then
  INST_GLIBC_PROGRAM=true
elif (($?>1)); then
  echo @@@STEP_FAILURE@@@
  exit 100
fi

echo @@@BUILD_STEP compile_toolchain@@@
(
  cd tools
  make -j8 buildbot-build-with-glibc
)

echo @@@BUILD_STEP tar_toolchain@@@
(
  cd tools
  tar zScf toolchain.tgz toolchain/ && chmod a+r toolchain.tgz
)

if [[ "$BUILDBOT_SLAVE_TYPE" != "Trybot" ]]; then
  echo @@@BUILD_STEP archive_build@@@
  /b/build/scripts/slave/gsutil -h Cache-Control:no-cache cp -a public-read \
    tools/toolchain.tgz \
    gs://nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/toolchain_mac_x86.tar.gz
  echo @@@STEP_LINK@download@http://gsdview.appspot.com/nativeclient-archive2/x86_toolchain/r${BUILDBOT_GOT_REVISION}/@@@
fi

echo @@@BUILD_STEP untar_toolchain@@@
(
  mkdir -p .tmp
  cd .tmp
  # GNU tar does not like some headers, regular tar can not create sparse files.
  # Use regular tar with non-sparse files for now.
  tar zxf ../tools/toolchain.tgz
  mv toolchain ..
)

echo @@@BUILD_STEP gyp_compile@@@
xcodebuild -project build/all.xcodeproj -configuration Release

RETCODE=0

echo @@@BUILD_STEP gyp_tests@@@
python trusted_test.py --config Release ||
  (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP scons_compile32@@@
./scons -k -j 8 DOXYGEN=../third_party/doxygen/osx/doxygen \
  --nacl_glibc --verbose --mode=opt-mac,nacl,doc platform=x86-32

echo @@@BUILD_STEP small_tests32@@@
./scons -k -j 8 \
  --mode=dbg-host,nacl platform=x86-32 \
  --nacl_glibc --verbose small_tests ||
  (RETCODE=$? && echo @@@STEP_FAILURE@@@)

# TODO(khim): add medium_tests, large_tests, chrome_browser_tests.

exit ${RETCODE}
