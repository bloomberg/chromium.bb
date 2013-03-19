#!/bin/sh
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Reads etc/ld.so.conf.d/*.conf and returns the appropriate linker flags.
#
#  sysroot_ld_path.sh /abspath/to/sysroot
#

if [ $# -ne 1 ]; then
  echo $0 /abspath/to/sysroot
  exit 1
fi

echo $1 | grep -qs ' '
if [ $? -eq 0 ]; then
  echo $1 contains whitespace.
  exit 1
fi

for entry in $(cat $1/etc/ld.so.conf.d/*.conf | grep ^/)
do
  entry=$1$entry
  echo -L$entry
  echo -Wl,-rpath-link=$entry
done | xargs echo
