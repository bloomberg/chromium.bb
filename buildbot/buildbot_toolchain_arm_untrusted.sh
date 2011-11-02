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

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo 'ERROR: must be run in native_client/ directory!'
  echo "       (Current directory is $(pwd))"
  exit 1
fi

if [ $# -ne 3 ]; then
  echo "USAGE: $0 [os] [arch] [libmode]"
  echo "os     : linux, mac, win"
  echo "arch   : 32, 64"
  echo "libmode: newlib, glibc"
  exit 2
fi

readonly BUILD_OS=$1
readonly BUILD_ARCH=$2
readonly BUILD_LIBMODE=$3

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

case ${BUILD_OS}-${BUILD_ARCH}-${BUILD_LIBMODE} in
  linux-32-newlib)
    # Don't test arm + 64-bit on 32-bit builder.
    # We can't build 64-bit trusted components on a 32-bit system.
    # Arm disabled on 32-bit because it runs out of memory.
    TOOLCHAIN_LABEL=pnacl_linux_i686_newlib
    RUN_TESTS="x86-32 x86-32-browser"
    ;;
  linux-32-glibc)
    TOOLCHAIN_LABEL=pnacl_linux_i686_glibc
    PNACL_BUILD="pnacl/build-glibc.sh"
    # TODO(pdox): Determine which tests should be run.
    RUN_TESTS=""
    ;;
  linux-64-newlib)
    TOOLCHAIN_LABEL=pnacl_linux_x86_64_newlib
    RUN_TESTS="x86-32 x86-32-browser"
    RUN_TESTS+=" arm arm-pic arm-browser"
    RUN_TESTS+=" x86-64 x86-64-browser"
    ;;
  linux-64-glibc)
    TOOLCHAIN_LABEL=pnacl_linux_x86_64_glibc
    PNACL_BUILD="pnacl/build-glibc.sh"
    RUN_TESTS=""
    RUN_TESTS="x86-32-glibc x86-32-browser-glibc"
    RUN_TESTS+=" x86-64-glibc x86-64-browser-glibc"
    ;;
  mac-32-newlib)
    export PNACL_VERBOSE=true  # To avoid timing out, since this bot is slow.
    # We can't test ARM because we do not have QEMU for Mac.
    # We can't test X86-64 because NaCl X86-64 Mac support is not in good shape.
    TOOLCHAIN_LABEL=pnacl_darwin_i386_newlib
    RUN_TESTS="x86-32 x86-32-browser"
    ;;
  mac-32-glibc)
    export PNACL_VERBOSE=true
    TOOLCHAIN_LABEL=pnacl_darwin_i386_glibc
    PNACL_BUILD="pnacl/build-glibc.sh"
    # TODO(pdox): Determine which tests should be run.
    RUN_TESTS=""
    ;;
  win-32-newlib)
    TOOLCHAIN_LABEL=pnacl_windows_i686_newlib
    RUN_TESTS="x86-32 x86-32-browser"
    ;;
  win-64-newlib)
    TOOLCHAIN_LABEL=pnacl_windows_i686_newlib
    BUILD_32BIT_PLUGIN=true
    RUN_TESTS="x86-64 x86-64-browser"
    ;;
  *)
    echo -n "*** UNRECOGNIZED CONFIGURATION: "
    echo "${BUILD_OS}-${BUILD_ARCH}-${BUILD_LIBMODE} ***"
    exit 3
esac

set -x

RETCODE=0

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out compiler ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib
# Try to clobber /tmp/ contents to clear temporary chrome files.
rm -rf /tmp/.org.chromium.Chromium.*

# Only clobber the directory of the toolchain being built.
rm -rf toolchain/${TOOLCHAIN_LABEL}
rm -rf toolchain/hg*
rm -rf toolchain/test-log

echo @@@BUILD_STEP show-config@@@
PNACL_BUILDBOT=true PNACL_PRUNE=true ${PNACL_BUILD} show-config

echo @@@BUILD_STEP compile_toolchain@@@
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
  gsutil=buildbot/gsutil.sh
  GS_BASE=gs://nativeclient-archive2/toolchain
  ${gsutil} -h Cache-Control:no-cache cp -a public-read \
      pnacl-toolchain.tgz \
      ${GS_BASE}/${BUILDBOT_GOT_REVISION}/naclsdk_${TOOLCHAIN_LABEL}.tgz
  ${gsutil} -h Cache-Control:no-cache cp -a public-read \
      pnacl-toolchain.tgz \
      ${GS_BASE}/latest/naclsdk_${TOOLCHAIN_LABEL}.tgz
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


if [[ ${RETCODE} != 0 ]]; then
  echo @@@BUILD_STEP summary@@@
  echo There were failed stages.
  exit ${RETCODE}
fi
