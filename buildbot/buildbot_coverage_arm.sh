#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

if [ $# -ne 0 ]; then
  echo "USAGE: $0"
  exit 2
fi

set -x
set -e
set -u


echo @@@BUILD_STEP clobber@@@
rm -rf scons-out ../xcodebuild ../sconsbuild ../out

echo @@@BUILD_STEP cleanup_temp@@@
ls -al /tmp/
rm -rf /tmp/* /tmp/.[!.]* || true

echo @@@BUILD_STEP scons_compile@@@
./scons -j 8 -k --verbose --mode=coverage-linux,nacl \
    platform=arm bitcode=1

echo @@@BUILD_STEP coverage@@@
./scons -k --verbose --mode=coverage-linux,nacl coverage platform=arm \
    bitcode=1

echo @@@BUILD_STEP archive_coverage@@@
export GSUTIL=/b/build/scripts/slave/gsutil
GSD_URL=http://gsdview.appspot.com/nativeclient-coverage2/revs
VARIANT_NAME=coverage-linux-arm
COVERAGE_PATH=${VARIANT_NAME}/html/index.html
BUILDBOT_REVISION=${BUILDBOT_REVISION:-None}
LINK_URL=${GSD_URL}/${BUILDBOT_REVISION}/${COVERAGE_PATH}
GSD_BASE=gs://nativeclient-coverage2/revs
GS_PATH=${GSD_BASE}/${BUILDBOT_REVISION}/${VARIANT_NAME}
/b/build/scripts/slave/gsutil_cp_dir.py -a public-read \
     scons-out/${VARIANT_NAME}/coverage ${GS_PATH}
echo @@@STEP_LINK@view@${LINK_URL}@@@
