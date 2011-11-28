#!/bin/bash

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# gcl wrapper to kick off nacl-sdk try jobs.

gcl try $* \
  -b naclsdkm-linux \
  -b naclsdkm-mac \
  -b naclsdkm-windows32 \
  -b naclsdkm-windows64 \
  -S svn://svn.chromium.org/chrome-try/try-nacl