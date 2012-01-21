#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -u
set -e

# On Windows, this script is invoked from a batch file.
# The inherited PWD environmental variable is a Windows-style path.
# This can cause problems with pwd and bash. This line fixes it.
cd -P .

readonly UP_DOWN_LOAD="buildbot/file_up_down_load.sh"

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo 'ERROR: must be run in native_client/ directory!'
  echo "       (Current directory is $(pwd))"
  exit 1
fi

if [ $# -ne 2 ]; then
  echo "USAGE: $0 [os] [arch]"
  echo "os     : linux, mac, win"
  echo "arch   : 32, 64"
  exit 2
fi

readonly BUILD_OS=$1
readonly BUILD_ARCH=$2

## Ignore this variable - used for testing only
readonly TEST_UPLOAD=${TEST_UPLOAD:-false}

echo "***            STARTING PNACL BUILD           ***"
if [ "${BUILDBOT_BUILDERNAME:+isset}" == "isset" ]; then
  echo "*** BUILDBOT_BUILDERNAME: ${BUILDBOT_BUILDERNAME}"
fi
echo "*** ARGUMENTS           : $*"

PNACL_BUILD="pnacl/build.sh"
PNACL_TEST="pnacl/test.sh"

# On some systems (e.g., windows 64-bit), we must build a 32-bit plugin
# because the browser is 32-bit. Only sel_ldr and the nexes are 64-bit.
BUILD_32BIT_PLUGIN=false

case ${BUILD_OS}-${BUILD_ARCH} in
  linux-32)
    # Don't test arm + 64-bit on 32-bit builder.
    # We can't build 64-bit trusted components on a 32-bit system.
    # Arm disabled on 32-bit because it runs out of memory.
    TOOLCHAIN_LABEL=pnacl_linux_i686
    RUN_TESTS="x86-32 x86-32-browser"
    ;;
  linux-64)
    TOOLCHAIN_LABEL=pnacl_linux_x86_64
    RUN_TESTS="x86-32 x86-32-browser"
    RUN_TESTS+=" x86-64 x86-64-browser"
    RUN_TESTS+=" x86-32-glibc x86-32-browser-glibc"
    RUN_TESTS+=" x86-64-glibc x86-64-browser-glibc"
    RUN_TESTS+=" arm arm-pic arm-browser"
    ;;
  mac-32)
    export PNACL_VERBOSE=true  # To avoid timing out, since this bot is slow.
    # We can't test ARM because we do not have QEMU for Mac.
    # We can't test X86-64 because NaCl X86-64 Mac support is not in good shape.
    TOOLCHAIN_LABEL=pnacl_darwin_i386
    RUN_TESTS="x86-32 x86-32-browser"
    ;;
  win-32)
    TOOLCHAIN_LABEL=pnacl_windows_i686
    RUN_TESTS="x86-32 x86-32-browser"
    ;;
  win-64)
    TOOLCHAIN_LABEL=pnacl_windows_i686
    BUILD_32BIT_PLUGIN=true
    RUN_TESTS="x86-64 x86-64-browser"
    ;;
  *)
    echo -n "*** UNRECOGNIZED CONFIGURATION: "
    echo "${BUILD_OS}-${BUILD_ARCH} ***"
    exit 3
esac

set -x

RETCODE=0

upload-cros-tarballs(){
  ## TODO(jasonwkim): convert this to using  UP_DOWN_LOAD script
  ## Runs only if completely successful on real buildbots only
  ## CrOS only cares about 64bit x86-64 as host platform
  ## so we only upload if we are botting for x86-64
  ${PNACL_BUILD} cros-tarball-all pnacl-src
  local gsutil=${GSUTIL:-buildbot/gsutil.sh}
  local GS_BASE="gs://pnacl-source/${BUILDBOT_GOT_REVISION}"

  for archive in pnacl-src/*.tbz2; do
    local dst="$(basename $archive)"
    echo Uploading ${archive}
    ${gsutil} -h Cache-Control:no-cache cp -a public-read \
      "${archive}" \
      "${GS_BASE}/${dst}";
  done
}

## Ignore this condition - for testing only
if ${TEST_UPLOAD}; then
  upload-cros-tarballs
  exit $?
fi

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out compiler ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib
# Try to clobber /tmp/ contents to clear temporary chrome files.
rm -rf /tmp/.org.chromium.Chromium.*

# Only clobber the directory of the toolchain being built.
# TODO(pdox): The * removes the old toolchain directories. It
# can be removed once all the buildbots have been cleaned.
rm -rf toolchain/${TOOLCHAIN_LABEL}*
rm -rf toolchain/hg*
rm -rf toolchain/test-log

echo @@@BUILD_STEP show-config@@@
PNACL_BUILDBOT=true PNACL_PRUNE=true ${PNACL_BUILD} show-config

echo @@@BUILD_STEP compile_toolchain@@@
rm -rf pnacl-toolchain.tgz pnacl/pnacl-toolchain.tgz
PNACL_BUILDBOT=true PNACL_PRUNE=true \
  ${PNACL_BUILD} untrusted_sdk pnacl-toolchain.tgz
chmod a+r pnacl-toolchain.tgz

echo @@@BUILD_STEP untar_toolchain@@@
# Untar to ensure we can and to place the toolchain where the main build
# expects it to be.
mkdir -p toolchain/${TOOLCHAIN_LABEL}
cd toolchain/${TOOLCHAIN_LABEL}
tar xfz ../../pnacl-toolchain.tgz
cd ../..

if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" != "Trybot" ]]; then
  echo @@@BUILD_STEP archive_build@@@
  ${UP_DOWN_LOAD} UploadArmUntrustedToolchains ${BUILDBOT_GOT_REVISION} \
    ${TOOLCHAIN_LABEL} pnacl-toolchain.tgz
fi

if ${BUILD_32BIT_PLUGIN}; then
  echo @@@BUILD_STEP plugin compile 32@@@
  ./scons --verbose -k -j8 --mode=opt-host,nacl platform=x86-32 plugin
fi

for arch in ${RUN_TESTS} ; do
  echo @@@BUILD_STEP test-${arch}@@@
  PNACL_BUILDBOT=true ${PNACL_TEST} test-${arch} -k ||
      { RETCODE=$? && echo @@@STEP_FAILURE@@@;}
done

# TODO: Remove this when we have a proper sdk for pnacl
# BUG: http://code.google.com/p/nativeclient/issues/detail?id=2547
echo @@@BUILD_STEP adhoc_sdk@@@
${PNACL_BUILD} sdk newlib
${PNACL_BUILD} ppapi-headers
${PNACL_BUILD} tarball pnacl-toolchain-adhoc-sdk.tgz
chmod a+r pnacl/pnacl-toolchain-adhoc-sdk.tgz
if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" != "Trybot" ]]; then
  echo @@@BUILD_STEP archive_build_adhoc_sdk@@@
  ${UP_DOWN_LOAD} UploadArmUntrustedToolchains ${BUILDBOT_GOT_REVISION} \
    ${TOOLCHAIN_LABEL}_adhoc_sdk pnacl/pnacl-toolchain-adhoc-sdk.tgz
fi

if [[ ${RETCODE} != 0 ]]; then
  echo @@@BUILD_STEP summary@@@
  echo There were failed stages.
  exit ${RETCODE}
fi

if [ "${BUILDBOT_SLAVE_TYPE:-Trybot}" != "Trybot" -a \
     "${TOOLCHAIN_LABEL}" == "pnacl_linux_x86_64" ]; then
  echo @@@BUILD_STEP cros_tarball@@@
  upload-cros-tarballs
fi
