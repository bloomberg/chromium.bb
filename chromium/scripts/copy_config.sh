#!/bin/bash -e

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Use this to copy all config files into the tree.
for os in linux mac; do
  for target in Chromium ChromiumOS Chrome ChromeOS; do
    # Copy config files for various architectures:
    #   - ia32/x64 have config.asm, config.h, codec_names.h
    #   - arm/arm-neon have config.h, codec_names.h
    for arch in arm arm-neon ia32 x64; do
      # Don't waste time on non-existent configs, if no config.h then skip.
      [ ! -e "build.$arch.$os/$target/config.h" ] && continue

      for f in config.h config.asm; do
        FROM="build.$arch.$os/$target/$f"
        TO="chromium/config/$target/$os/$arch/$f"
        [ -e $FROM ] && cp -v $FROM $TO
      done
    done
  done
done

echo "Copied all existing newer configs successfully."
