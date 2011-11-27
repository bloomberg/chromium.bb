#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all of the ChromeDriver tests.

For ChromeDriver documentation, refer to:
  http://dev.chromium.org/developers/testing/webdriver-for-chrome
"""

import optparse
import os
import sys
import unittest

import test_paths

sys.path += [test_paths.PYTHON_BINDINGS]
sys.path += [test_paths.SRC_THIRD_PARTY]

from chromedriver_test import ChromeDriverTest
import chromedriver_tests
import py_unittest_util


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--filter', type='string', default='*',
      help='Filter for specifying what tests to run, google test style.')
  parser.add_option(
      '', '--driver-exe', type='string', default=None,
      help='Path to the default ChromeDriver executable to use.')
  parser.add_option(
      '', '--chrome-exe', type='string', default=None,
      help='Path to the default Chrome executable to use.')
  parser.add_option(
      '', '--list', action='store_true', default=False,
      help='List tests instead of running them.')
  options, args = parser.parse_args()

  all_tests_suite = unittest.defaultTestLoader.loadTestsFromModule(
      chromedriver_tests)
  filtered_suite = py_unittest_util.FilterTestSuite(
      all_tests_suite, options.filter)

  if options.list is True:
    print '\n'.join(py_unittest_util.GetTestNamesFromSuite(filtered_suite))
    return 0
  if sys.platform.startswith('darwin'):
    print 'All tests temporarily disabled on mac, crbug.com/103434'
    return 0

  driver_exe = options.driver_exe
  if driver_exe is not None:
    driver_exe = os.path.expanduser(options.driver_exe)
  chrome_exe = options.chrome_exe
  if chrome_exe is not None:
    chrome_exe = os.path.expanduser(options.chrome_exe)
  ChromeDriverTest.GlobalSetUp(driver_exe, chrome_exe)
  result = py_unittest_util.GTestTextTestRunner(verbosity=1).run(filtered_suite)
  ChromeDriverTest.GlobalTearDown()
  return not result.wasSuccessful()


if __name__ == '__main__':
  sys.exit(main())
