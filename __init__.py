# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

# Add the third_party/ dir to our search path so that we can find the
# modules in there automatically.  This isn't normal, so don't replicate
# this pattern elsewhere.
_third_party = os.path.normpath(os.path.join(os.path.dirname(os.path.realpath(
    __file__)), 'third_party'))
sys.path.insert(0, _third_party)

# List of third_party packages that might need subpaths added to search.
_paths = [
    'pyelftools',
]
for _path in _paths:
  sys.path.insert(1, os.path.join(_third_party, _path))

