#!/bin/bash -e

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use this to copy all config files into the tree.

# Linux (all combinations).
for target in Chromium ChromiumOS Chrome ChromeOS; do
  # ia32/x64 have this funny little config.asm file!
  for arch in ia32 x64; do
    for f in config.h config.asm; do
      FROM="build.$arch.linux/$target/$f"
      TO="source/config/$target/linux/$arch/$f"
      [ -e $FROM ] && cp -u -v $FROM $TO
    done
  done

  # arm/arm-neon only have a config.h.
  for arch in arm arm-neon; do
    FROM="build.$arch.linux/$target/config.h"
    TO="source/config/$target/linux/$arch/config.h"
    [ -e $FROM ] && cp -u -v $FROM $TO
  done
done

# Mac/Windows (ia32).
for os in mac win; do
  for target in Chromium Chrome ; do
    for f in config.h config.asm; do
      FROM="build.ia32.$os/$target/$f"
      TO="source/config/$target/$os/ia32/$f"
      [ -e $FROM ] && cp -u -v $FROM $TO
    done
  done
done

echo "Copied all existing newer configs successfully."
