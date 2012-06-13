#!/bin/bash

# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Get the list of nacl-sdk try bots.

echo "-b naclsdkm-mac \
  -b naclsdkm-linux \
  -b naclsdkm-pnacl-linux \
  -b naclsdkm-windows32 \
  -b naclsdkm-windows64 \
  -S svn://svn.chromium.org/chrome-try/try-nacl"
