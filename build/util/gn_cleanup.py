# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

# This script is run as a DEPS hook, so the cwd is where .gclient is.
for p in (('linux64/gn', 'mac/gn', 'win/gn.exe')):
  in_buildtools = os.path.join('src', 'buildtools', p)
  if os.path.isfile(in_buildtools):
    os.unlink(in_buildtools)
