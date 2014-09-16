# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This is really a helper for other scripts that need a PYTHONPATH entry
   for crostools.

   Generally used by import statements of the form:
     from chromite.lib.paygen import foo
     from crostools.scripts import foo
"""

from __future__ import print_function

import os.path
import sys


# Find the correct root path to insert into sys.path for importing
# modules in this source.
CROSTOOLS_ROOT = os.path.realpath(
  os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', '..'))

CROSTOOLS_PATH_ROOT = os.path.dirname(CROSTOOLS_ROOT)

CROS_SRC_PLATFORM_PATH = os.path.join(CROSTOOLS_PATH_ROOT, 'src', 'platform')

CROS_AUTOTEST_PATH = os.path.join(CROSTOOLS_PATH_ROOT, 'src', 'third_party',
                                  'autotest', 'files')


def _SysPathPrepend(dir_name):
  """Prepend a directory to Python's import path."""
  if os.path.isdir(dir_name) and dir_name not in sys.path:
    sys.path.insert(0, dir_name)


def FixupPath():
  _SysPathPrepend(CROS_AUTOTEST_PATH)
  _SysPathPrepend(CROS_SRC_PLATFORM_PATH)
  _SysPathPrepend(CROSTOOLS_PATH_ROOT)


# TODO(dgarrett): Remove this call after all importers do it locally.
FixupPath()
