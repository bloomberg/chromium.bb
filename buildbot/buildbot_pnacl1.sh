#!/bin/bash

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

set -x
set -e
set -u


SCONS_SDK_COMMON="./scons --mode=nacl_extra_sdk --verbose bitcode=1 --download"
SCONS_COMMON="./scons --mode=opt-linux,nacl --verbose bitcode=1 -j8 -k"


echo @@@BUILD_STEP gclient_runhooks@@@
gclient runhooks --force

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out toolchain compiler hg ../xcodebuild ../sconsbuild ../out \
    src/third_party/nacl_sdk/arm-newlib

for platform in arm x86-32 x86-64 ; do
  echo @@@BUILD_STEP partial_sdk [${platform}]@@@
  ${SCONS_SDK_COMMON} platform=${platform} \
    extra_sdk_update_header install_libpthread extra_sdk_update

  echo @@@BUILD_STEP scons smoke_tests [${platform}]@@@
  ${SCONS_COMMON} platform=${platform} smoke_tests

  echo @@@BUILD_STEP scons smoke_tests pic [${platform}]@@@
  ${SCONS_COMMON} platform=${platform} smoke_tests nacl_pic=1

  if [ "${platform}" != "arm" ] ; then
    echo @@@BUILD_STEP scons smoke_tests translator [${platform}]@@@
    ${SCONS_COMMON} platform=${platform} smoke_tests use_sandboxed_translator=1
  fi
done

