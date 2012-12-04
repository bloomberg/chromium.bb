# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""End to end tests for ChromeDriver."""

import ctypes
import os
import sys
import unittest

import chromedriver


class ChromeDriverTest(unittest.TestCase):
  """End to end tests for ChromeDriver."""

  def testStartStop(self):
    driver = chromedriver.ChromeDriver(_CHROMEDRIVER_LIB, _CHROME_BINARY)
    driver.Quit()

  def testLoadUrl(self):
    driver = chromedriver.ChromeDriver(_CHROMEDRIVER_LIB)
    driver.Load('http://www.google.com')
    driver.Quit()


if __name__ == '__main__':
  if len(sys.argv) != 2 and len(sys.argv) != 3:
    print ('Usage: %s <path_to_chromedriver_so> [path_to_chrome_binary]' %
           __file__)
    sys.exit(1)
  global _CHROMEDRIVER_LIB
  _CHROMEDRIVER_LIB = os.path.abspath(sys.argv[1])
  global _CHROME_BINARY
  if len(sys.argv) == 3:
    _CHROME_BINARY = os.path.abspath(sys.argv[2])
  else:
    _CHROME_BINARY = None
  all_tests_suite = unittest.defaultTestLoader.loadTestsFromModule(
      sys.modules[__name__])
  result = unittest.TextTestRunner().run(all_tests_suite)
  sys.exit(len(result.failures) + len(result.errors))
