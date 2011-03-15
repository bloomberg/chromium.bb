#!/bin/bash

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

if [ $# -ne 1 ]; then
  echo "USAGE: $0 dbg/opt"
  exit 2
fi

set -x
set -e
set -u

# Pick dbg or opt
MODE=$1

RETCODE=0

if [[ $MODE == dbg ]]; then
  GYPMODE=Debug
else
  GYPMODE=Release
fi

echo @@@BUILD_STEP gclient_runhooks@@@
gclient runhooks --force

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out toolchain compiler hg ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib

echo @@@BUILD_STEP partial_sdk@@@
./scons --verbose --mode=nacl_extra_sdk platform=x86-32 --download \
extra_sdk_update_header install_libpthread extra_sdk_update

echo @@@BUILD_STEP gyp_compile@@@
xcodebuild -project build/all.xcodeproj -configuration ${GYPMODE}

echo @@@BUILD_STEP gyp_tests@@@
python trusted_test.py --config ${GYPMODE}

echo @@@BUILD_STEP scons_compile@@@
./scons -j 8 DOXYGEN=../third_party/doxygen/osx/doxygen -k --verbose \
    --mode=${MODE}-mac,nacl,doc platform=x86-32

echo @@@BUILD_STEP small_tests@@@
./scons DOXYGEN=../third_party/doxygen/osx/doxygen -k --verbose \
    --mode=${MODE}-mac,nacl,doc small_tests platform=x86-32 || \
    (RETCODE=$? && echo @@@BUILD_FAILED@@@)

echo @@@BUILD_STEP medium_tests@@@
./scons DOXYGEN=../third_party/doxygen/osx/doxygen -k --verbose \
    --mode=${MODE}-mac,nacl,doc medium_tests platform=x86-32 || \
    (RETCODE=$? && echo @@@BUILD_FAILED@@@)

echo @@@BUILD_STEP large_tests@@@
./scons DOXYGEN=../third_party/doxygen/osx/doxygen -k --verbose \
    --mode=${MODE}-mac,nacl,doc large_tests platform=x86-32 || \
    (RETCODE=$? && echo @@@BUILD_FAILED@@@)

echo @@@BUILD_STEP begin_browser_testing@@@
./scons DOXYGEN=../third_party/doxygen/osx/doxygen -k --verbose \
    --mode=${MODE}-mac,nacl,doc SILENT=1 platform=x86-32 \
    firefox_remove

echo @@@BUILD_STEP chrome_browser_tests@@@
./scons DOXYGEN=../third_party/doxygen/osx/doxygen -k --verbose \
    --mode=${MODE}-mac,nacl,doc SILENT=1 platform=x86-32 \
    chrome_browser_tests || \
    (RETCODE=$? && echo @@@BUILD_FAILED@@@)

echo @@@BUILD_STEP install_plugin@@@
./scons DOXYGEN=../third_party/doxygen/osx/doxygen -k --verbose \
    --mode=${MODE}-mac,nacl,doc SILENT=1 platform=x86-32 firefox_install

echo @@@BUILD_STEP selenium@@@
./scons DOXYGEN=../third_party/doxygen/osx/doxygen -k --verbose \
    --mode=${MODE}-mac,nacl,doc SILENT=1 platform=x86-32 browser_tests || \
    (RETCODE=$? && echo @@@BUILD_FAILED@@@)

echo @@@BUILD_STEP end_browser_testing@@@
./scons DOXYGEN=../third_party/doxygen/osx/doxygen -k --verbose \
    --mode=${MODE}-mac,nacl,doc SILENT=1 platform=x86-32 \
    firefox_remove

exit ${RETCODE}
