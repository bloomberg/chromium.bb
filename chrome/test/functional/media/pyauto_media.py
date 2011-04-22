#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Setup for PyAuto media tests.

Use the following in your scripts to run them standalone:

# This should be at the top
import pyauto_media

if __name__ == '__main__':
  pyauto_media.Main()

This script looks similar to pyauto_functional.py. However, unlike
pyauto_functional, this script can NOT be used as an executable to
fire off other media scripts since media tests require additional
parameters to be set in the form of environment variables (unless you
want these variables to be defaults, which is generally not a good
idea).
"""

import os
import sys


def _SetupPaths():
  """Setting path to find pyauto_functional.py."""
  media_dir = os.path.dirname(__file__)
  sys.path.append(media_dir)
  sys.path.append(os.path.join(media_dir, os.pardir))

_SetupPaths()

import pyauto_functional
import pyauto


class Main(pyauto_functional.Main):
  """Main program for running PyAuto media tests."""

  def __init__(self):
    pyauto_functional.Main.__init__(self)


if __name__ == '__main__':
  Main()
