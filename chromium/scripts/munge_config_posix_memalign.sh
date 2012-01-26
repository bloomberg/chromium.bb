# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script munges the config.h to mark posix_memalign unavailable so 
# that the ffmpeg library can be linked against stdlib for 
# MACOSX_DEPLOYMENT_TARGET < 10.7
# This is because we are building for older systems which did not have
# this functionality.
#
# Warning: in theory we should also disable this on config.asm but it is
# not yet used there.
set -e

sed -i.orig -e '
/HAVE_POSIX_MEMALIGN/ {
c\
#define HAVE_POSIX_MEMALIGN 0
}
' \
$1
