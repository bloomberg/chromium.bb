# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script munges the config.h and config.asm to mark iconv library as
# unavailable so that the ffmpeg library can be used on systems that don't
# have iconv installed.

set -e

sed -i.orig -e 's/CONFIG_ICONV 1/CONFIG_ICONV 0/g' $1
