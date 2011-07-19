#!/usr/bin/python

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Setup for PyAuto functional tests.

Use the following in your scripts to run them standalone:

# This should be at the top
import pyauto_functional

if __name__ == '__main__':
  pyauto_functional.Main()

This script can be used as an executable to fire off other scripts, similar
to unittest.py
  python pyauto_functional.py test_script
"""

import os
import sys

def _LocatePyAutoDir():
  sys.path.append(os.path.join(os.path.dirname(__file__),
                               os.pardir, 'pyautolib'))

_LocatePyAutoDir()

try:
  import pyauto
except ImportError:
  print >>sys.stderr, 'Cannot import pyauto from %s' % sys.path
  raise


class Main(pyauto.Main):
  """Main program for running PyAuto functional tests."""

  def __init__(self):
    # Make scripts in this dir importable
    sys.path.append(os.path.dirname(__file__))
    pyauto.Main.__init__(self)

  def TestsDir(self):
    return os.path.dirname(__file__)


if __name__ == '__main__':
  Main()

