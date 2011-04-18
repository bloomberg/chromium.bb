#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

if [ $# -ne 3 ]; then
  echo "USAGE: $0 dbg/opt 32/64 newlib/glibc"
  exit 2
fi

set -x
set -e
set -u

# Pick dbg or opt
MODE=$1
# Pick 32 or 64
BITS=$2
# Pick glibc or newlib
TOOLCHAIN=$3

RETCODE=0

if [[ $MODE == dbg ]]; then
  GYPMODE=Debug
else
  GYPMODE=Release
fi

if [[ $TOOLCHAIN = glibc ]]; then
  GLIBCOPTS="--nacl_glibc"
else
  GLIBCOPTS=""
fi

# Skip over hooks, clobber, and partial_sdk when run inside the toolchain build
# as the toolchain takes care or the clobber, hooks aren't needed, and
# partial_sdk really shouldn't be needed.
if [[ "${INSIDE_TOOLCHAIN:-}" == "" ]]; then

echo @@@BUILD_STEP gclient_runhooks@@@
gclient runhooks --force

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out toolchain compiler hg ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib

echo @@@BUILD_STEP partial_sdk${BITS}@@@
if [[ $TOOLCHAIN = glibc ]]; then
  buildbot/download_glibc_toolchain.sh linux ${BITS}
else
  ./scons --verbose --mode=nacl_extra_sdk platform=x86-${BITS} --download \
  extra_sdk_update_header install_libpthread extra_sdk_update
fi

fi

echo @@@BUILD_STEP gyp_compile@@@
make -C .. -k -j12 V=1 BUILDTYPE=${GYPMODE}

echo @@@BUILD_STEP gyp_tests@@@
python trusted_test.py --config ${GYPMODE}

echo @@@BUILD_STEP scons_compile${BITS}@@@
./scons -j 8 DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc platform=x86-${BITS}

echo @@@BUILD_STEP small_tests${BITS}@@@
./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc small_tests \
    platform=x86-${BITS} ||
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

# TODO(khim): run other tests with glibc toolchain
if [[ $TOOLCHAIN != glibc ]]; then
echo @@@BUILD_STEP medium_tests${BITS}@@@
./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc medium_tests \
    platform=x86-${BITS} ||
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP large_tests${BITS}@@@
./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc large_tests \
    platform=x86-${BITS} ||
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

if [[ "${INSIDE_TOOLCHAIN:-}" == "" ]]; then

echo @@@BUILD_STEP chrome_browser_tests@@@
# Although we could use the "browser_headless=1" Scons option, it runs
# xvfb-run once per Chromium invocation.  This is good for isolating
# the tests, but xvfb-run has a stupid fixed-period sleep, which would
# slow down the tests unnecessarily.
XVFB_PREFIX="xvfb-run --auto-servernum"

$XVFB_PREFIX \
    ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc SILENT=1 platform=x86-${BITS} \
    chrome_browser_tests ||
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

echo @@@BUILD_STEP pyauto_tests${BITS}@@@
$XVFB_PREFIX \
    ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc SILENT=1 platform=x86-${BITS} \
    pyauto_tests ||
    (RETCODE=$? && echo @@@STEP_FAILURE@@@)

fi
fi

exit ${RETCODE}
