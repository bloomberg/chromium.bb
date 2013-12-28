# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script just returns the environment variable GOMA_DIR if there is one,
# which is the default goma location on Android. Returns the empty string if
# it's not defined.
#
# TODO(brettw) this should be rationalized with the way that other systems get
# their goma setup.

import os

if 'GOMA_DIR' in os.environ:
  print '"' + os.environ['GOMA_DIR'] + '"'
else:
  print '""'
