#!/bin/bash
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script assumed to be run in native_client/
if [[ $(pwd) != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

if [ $# -ne 1 ]; then
  echo "USAGE: $0 32/64"
  exit 2
fi

set -x
set -e
set -u

# Pick 32 or 64
BITS=$1


echo @@@BUILD_STEP clobber@@@
rm -rf scons-out ../sconsbuild ../out

echo @@@BUILD_STEP cleanup_temp@@@
ls -al /tmp/
rm -rf /tmp/* /tmp/.[!.]* || true

echo @@@BUILD_STEP scons_compile@@@
./scons -j 8 -k --verbose --mode=coverage-linux,nacl platform=x86-${BITS}

echo @@@BUILD_STEP coverage@@@
XVFB_PREFIX="xvfb-run --auto-servernum"

$XVFB_PREFIX \
    ./scons -k --verbose --mode=coverage-linux,nacl coverage \
    platform=x86-${BITS}
python tools/coverage_linux.py ${BITS}

echo @@@BUILD_STEP archive_coverage@@@
export GSUTIL=/b/build/scripts/slave/gsutil
GSD_URL=http://gsdview.appspot.com/nativeclient-coverage2/revs
VARIANT_NAME=coverage-linux-x86-${BITS}
COVERAGE_PATH=${VARIANT_NAME}/html/index.html
BUILDBOT_REVISION=${BUILDBOT_REVISION:-None}
LINK_URL=${GSD_URL}/${BUILDBOT_REVISION}/${COVERAGE_PATH}
GSD_BASE=gs://nativeclient-coverage2/revs
GS_PATH=${GSD_BASE}/${BUILDBOT_REVISION}/${VARIANT_NAME}
/b/build/scripts/slave/gsutil_cp_dir.py -a public-read \
     scons-out/${VARIANT_NAME}/coverage ${GS_PATH}
echo @@@STEP_LINK@view@${LINK_URL}@@@
