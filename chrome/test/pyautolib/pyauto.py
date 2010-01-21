#!/usr/bin/python

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""PyAuto: Python Interface to Chromium's Automation Proxy.

PyAuto uses swig to expose Automation Proxy interfaces to Python.
For complete documentation on the functionality available,
run pydoc on this file.

Ref: http://dev.chromium.org/developers/pyauto
"""

import os
import sys
import unittest


def _LocateBinDirs():
  script_dir = os.path.dirname(__file__)
  chrome_src = os.path.join(script_dir, os.pardir, os.pardir, os.pardir)

  # TODO(nirnimesh): Expand this to include win/linux build dirs
  #                  crbug.com/32285
  bin_dirs = [ os.path.join(chrome_src, 'xcodebuild', 'Debug'),
               os.path.join(chrome_src, 'xcodebuild', 'Release'),
             ]
  for d in bin_dirs:
    sys.path.append(os.path.normpath(d))

_LocateBinDirs()

try:
  from pyautolib import PyUITestSuite
except ImportError:
  print >>sys.stderr, "Could not locate built libraries. Did you build?"
  raise


class PyUITest(PyUITestSuite, unittest.TestCase):
  """Base class for UI Test Cases in Python.

  A browser is created before executing each test, and is destroyed after
  each test irrespective of whether the test passed or failed.

  You should derive from this class and create methods with 'test' prefix,
  and use methods inherited from PyUITestSuite (the C++ side).

  Example:

    class MyTest(PyUITest):

      def testNavigation(self):
        self.NavigateToURL("http://www.google.com")
        self.assertTrue("Google" == self.GetActiveTabTitle())
  """

  def __init__(self, methodName='runTest'):
    PyUITestSuite.__init__(self, sys.argv)
    unittest.TestCase.__init__(self, methodName)

  def __del__(self):
    PyUITestSuite.__del__(self)

  def run(self, result=None):
    """The main run method.

    We override this method to make calls to the setup steps in PyUITestSuite.
    """
    self.SetUp()     # Open a browser window
    unittest.TestCase.run(self, result)
    self.TearDown()  # Destroy the browser window
