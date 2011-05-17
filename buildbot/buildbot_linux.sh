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

if [[ $BITS == 32 ]]; then
  GYPARCH=ia32
else
  GYPARCH=x64
fi
export GYP_DEFINES=target_arch=${GYPARCH}

if [[ $TOOLCHAIN = glibc ]]; then
  GLIBCOPTS="--nacl_glibc"
else
  GLIBCOPTS=""
fi

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out hg ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib

# Skip over hooks and partial_sdk when run inside the toolchain build
# as the toolchain takes care or the clobber, hooks aren't needed, and
# partial_sdk really shouldn't be needed.
if [[ "${INSIDE_TOOLCHAIN:-}" == "" ]]; then
  echo @@@BUILD_STEP gclient_runhooks@@@
  gclient runhooks --force

  echo @@@BUILD_STEP partial_sdk${BITS}@@@
  if [[ $TOOLCHAIN = glibc ]]; then
    ./scons --verbose --mode=nacl_extra_sdk platform=x86-${BITS} --download \
    --nacl_glibc extra_sdk_update_header extra_sdk_update
  else
    ./scons --verbose --mode=nacl_extra_sdk platform=x86-${BITS} --download \
    extra_sdk_update_header install_libpthread extra_sdk_update
  fi
else
  # GYP_DEFINES tells GYP whether we need x86-32 or x86-64 binaries to
  # generate in the gyp_compile stage.  On toolchain bot we can not just
  # use gclient runhooks --force because it'll clobber freshly created
  # toolchain.
  echo @@@BUILD_STEP gyp_generate_only@@@
  cd ..
  native_client/build/gyp_nacl native_client/build/all.gyp
  cd native_client
fi

echo @@@BUILD_STEP gyp_compile@@@
make -C .. -k -j12 V=1 BUILDTYPE=${GYPMODE}

echo @@@BUILD_STEP gyp_tests@@@
python trusted_test.py --config ${GYPMODE} --bits ${BITS}

echo @@@BUILD_STEP scons_compile${BITS}@@@
./scons -j 8 DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc platform=x86-${BITS}

echo @@@BUILD_STEP small_tests${BITS}@@@
./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc small_tests \
    platform=x86-${BITS} ||
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}

if [[ $TOOLCHAIN = glibc ]]; then
echo @@@BUILD_STEP dynamic_library_browser_tests${BITS}@@@
    ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    browser_headless=1 \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc SILENT=1 platform=x86-${BITS} \
    dynamic_library_browser_tests ||
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}
fi

# TODO(khim): run other tests with glibc toolchain
if [[ $TOOLCHAIN != glibc ]]; then
echo @@@BUILD_STEP medium_tests${BITS}@@@
./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc medium_tests \
    platform=x86-${BITS} ||
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}

echo @@@BUILD_STEP large_tests${BITS}@@@
./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc large_tests \
    platform=x86-${BITS} ||
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}

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
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}

echo @@@BUILD_STEP chrome_browser_tests using GYP@@@
$XVFB_PREFIX \
    ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc SILENT=1 platform=x86-${BITS} \
    force_ppapi_plugin=../out/${GYPMODE}/lib.target/libppGoogleNaClPlugin.so \
    force_sel_ldr=../out/${GYPMODE}/sel_ldr \
    chrome_browser_tests ||
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}

# TODO(mseaborn): Drop support for non-IRT builds so that this is the
# default.  See http://code.google.com/p/nativeclient/issues/detail?id=1691
echo @@@BUILD_STEP chrome_browser_tests using IRT@@@
$XVFB_PREFIX \
    ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc SILENT=1 platform=x86-${BITS} \
    chrome_browser_tests irt=1 ||
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}

echo @@@BUILD_STEP pyauto_tests${BITS}@@@
$XVFB_PREFIX \
    ./scons DOXYGEN=../third_party/doxygen/linux/doxygen -k --verbose \
    ${GLIBCOPTS} --mode=${MODE}-linux,nacl,doc SILENT=1 platform=x86-${BITS} \
    pyauto_tests ||
    { RETCODE=$? && echo @@@STEP_FAILURE@@@;}

echo @@@BUILD_STEP archive irt.nexe@@@
# Upload the integrated runtime (IRT) library so that it can be pulled
# into the Chromium build, so that a NaCl toolchain will not be needed
# to build a NaCl-enabled Chromium.  We strip the IRT to save space
# and download time.
# TODO(mseaborn): It might be better to do the stripping in Scons.
IRT_PATH=scons-out/nacl-x86-${BITS}/staging/irt.nexe
toolchain/linux_x86_newlib/bin/nacl-strip \
    --strip-debug ${IRT_PATH} -o ${IRT_PATH}.stripped

function gscp() {
  GSUTIL=/b/build/scripts/slave/gsutil
  ${GSUTIL} -h Cache-Control:no-cache cp -a public-read "$@"
}

if [ "${ARCHIVE_IRT:-}" = "1" ]; then
  IRT_DIR=nativeclient-archive2/irt
  GSDVIEW=http://gsdview.appspot.com
  GS_PATH=${IRT_DIR}/r${BUILDBOT_GOT_REVISION}/irt_x86_${BITS}.nexe
  gscp ${IRT_PATH}.stripped gs://${GS_PATH}
  echo @@@STEP_LINK@stripped@${GSDVIEW}/${GS_PATH}@@@
  gscp ${IRT_PATH} gs://${GS_PATH}.unstripped
  echo @@@STEP_LINK@unstripped@${GSDVIEW}/${GS_PATH}.unstripped@@@
else
  echo @@@STEP_TEXT@not uploading on this bot@@@
fi

fi
fi

if [[ ${RETCODE} != 0 ]]; then
  echo @@@BUILD_STEP summary@@@
  echo There were failed stages.
  exit ${RETCODE}
fi
