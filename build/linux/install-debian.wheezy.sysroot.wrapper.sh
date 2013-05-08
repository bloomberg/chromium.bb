#!/bin/sh
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This wrapper script runs the Debian sysroot installation scripts, if they
# exist.
#
# The script is a no-op except for linux users who have the following in their
# GYP_DEFINES:
#
# * branding=Chrome
# * buildtype=Official
# * target_arch=[matching_arch]
#
# and not:
#
# * chromeos=1

set -e

SRC_DIR="$(dirname "$0")/../../"
SCRIPT_DIR="chrome/installer/linux/internal/sysroot_scripts/"
SCRIPT_FILE="$SRC_DIR/$SCRIPT_DIR/install-debian.wheezy.sysroot.py"

if [ -e "$SCRIPT_FILE" ]; then
  python "$SCRIPT_FILE" --linux-only --arch=amd64
  python "$SCRIPT_FILE" --linux-only --arch=i386
fi
