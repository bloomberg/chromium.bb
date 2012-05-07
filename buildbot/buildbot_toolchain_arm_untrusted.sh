#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -o xtrace
set -o nounset
set -o errexit

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

IS_TRYBOT=false
if [[ "${BUILDBOT_SLAVE_TYPE:-Trybot}" == "Trybot" ]]; then
  IS_TRYBOT=true
fi

echo "***            STARTING PNACL BUILD           ***"
if [ "${BUILDBOT_BUILDERNAME:+isset}" == "isset" ]; then
  echo "*** BUILDBOT_BUILDERNAME: ${BUILDBOT_BUILDERNAME}"
fi
echo "*** ARGUMENTS           : $*"

PNACL_BUILD="pnacl/build.sh"
PNACL_TEST="pnacl/test.sh"

# Build and upload the translator tarball. Only one builder should do this.
TRANSLATOR_BOT=false

# Tell build.sh and test.sh that we're a bot.
export PNACL_BUILDBOT=true

# Tells build.sh to prune the install directory (for release).
export PNACL_PRUNE=true

# On some systems (e.g., windows 64-bit), we must build a 32-bit plugin
# because the browser is 32-bit. Only sel_ldr and the nexes are 64-bit.
BUILD_32BIT_PLUGIN=false

case ${BUILD_OS}-${BUILD_ARCH} in
  linux-32)
    # Don't test arm + 64-bit on 32-bit builder.
    # We can't build 64-bit trusted components on a 32-bit system.
    # Arm disabled on 32-bit because it runs out of memory.
    TOOLCHAIN_LABEL=pnacl_linux_x86_32
    RUN_TESTS="x86-32 x86-32-browser"
    ;;
  linux-64)
    TRANSLATOR_BOT=true
    TOOLCHAIN_LABEL=pnacl_linux_x86_64
    RUN_TESTS="x86-32 x86-32-browser"
    RUN_TESTS+=" x86-64 x86-64-browser"
    RUN_TESTS+=" x86-32-glibc x86-32-browser-glibc"
    RUN_TESTS+=" x86-64-glibc x86-64-browser-glibc"
    RUN_TESTS+=" arm arm-browser"
    ;;
  mac-32)
    export PNACL_VERBOSE=true  # To avoid timing out, since this bot is slow.
    # We can't test ARM because we do not have QEMU for Mac.
    # We can't test X86-64 because NaCl X86-64 Mac support is not in good shape.
    TOOLCHAIN_LABEL=pnacl_mac_x86_32
    RUN_TESTS="x86-32 x86-32-browser"
    ;;
  win-32)
    TOOLCHAIN_LABEL=pnacl_win_x86_32
    RUN_TESTS="x86-32 x86-32-browser"
    ;;
  win-64)
    TOOLCHAIN_LABEL=pnacl_win_x86_32
    BUILD_32BIT_PLUGIN=true
    RUN_TESTS="x86-64 x86-64-browser"
    ;;
  *)
    echo -n "*** UNRECOGNIZED CONFIGURATION: "
    echo "${BUILD_OS}-${BUILD_ARCH} ***"
    exit 3
esac

clobber-chrome-profiles() {
  # Try to clobber /tmp/ contents to clear temporary chrome files.
  rm -rf /tmp/.org.chromium.Chromium.*
}

RETCODE=0

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out ../xcodebuild ../sconsbuild ../out
clobber-chrome-profiles
rm -rf toolchain/${TOOLCHAIN_LABEL}
rm -rf toolchain/test-log
rm -rf pnacl*.tgz pnacl/pnacl*.tgz
if ${TRANSLATOR_BOT}; then
  rm -rf toolchain/pnacl_translator*
fi

echo @@@BUILD_STEP show-config@@@
${PNACL_BUILD} show-config

echo @@@BUILD_STEP compile_toolchain@@@
${PNACL_BUILD} clean
${PNACL_BUILD} everything
${PNACL_BUILD} tarball pnacl-toolchain.tgz
chmod a+r pnacl-toolchain.tgz

echo @@@BUILD_STEP untar_toolchain@@@
# Untar to ensure we can and to place the toolchain where the main build
# expects it to be.
rm -rf toolchain/${TOOLCHAIN_LABEL}
mkdir -p toolchain/${TOOLCHAIN_LABEL}
cd toolchain/${TOOLCHAIN_LABEL}
tar xfz ../../pnacl-toolchain.tgz
cd ../..

if ${TRANSLATOR_BOT}; then
  echo @@@BUILD_STEP compile_translator@@@
  ${PNACL_BUILD} translator-clean-all
  ${PNACL_BUILD} translator-all
fi

if ! ${IS_TRYBOT}; then
  echo @@@BUILD_STEP archive_toolchain@@@
  ${UP_DOWN_LOAD} UploadArmUntrustedToolchains ${BUILDBOT_GOT_REVISION} \
    ${TOOLCHAIN_LABEL} pnacl-toolchain.tgz

  if ${TRANSLATOR_BOT}; then
    echo @@@BUILD_STEP archive_translator@@@
    ${PNACL_BUILD} translator-tarball pnacl-translator.tgz
    ${UP_DOWN_LOAD} UploadArmUntrustedToolchains ${BUILDBOT_GOT_REVISION} \
      pnacl_translator pnacl-translator.tgz
  fi
fi

if ${BUILD_32BIT_PLUGIN}; then
  echo @@@BUILD_STEP plugin compile 32@@@
  ./scons --verbose -k -j8 --mode=opt-host,nacl platform=x86-32 plugin
fi

for arch in ${RUN_TESTS} ; do
  # Compensate for the fact that the the *other* script already prints
  # a step message for the browser case.
  # BUG: http://code.google.com/p/nativeclient/issues/detail?id=2768
  if [[ ${arch} != *-browser* ]]
    echo @@@BUILD_STEP test-${arch}@@@
  fi
  ${PNACL_TEST} test-${arch} -k || { RETCODE=$? && echo @@@STEP_FAILURE@@@; }
  # Be sure to free up some /tmp between test stages.
  clobber-chrome-profiles
done

if [[ ${RETCODE} != 0 ]]; then
  echo @@@BUILD_STEP summary@@@
  echo There were failed stages.
  exit ${RETCODE}
fi
