#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs all of the ChromeDriver tests.

For ChromeDriver documentation, refer to:
  http://dev.chromium.org/developers/testing/webdriver-for-chrome
"""

import fnmatch
import optparse
import sys
import unittest

from gtest_text_test_runner import GTestTextTestRunner
import test_paths

sys.path += [test_paths.PYTHON_BINDINGS]
sys.path += [test_paths.SRC_THIRD_PARTY]

import chromedriver_tests
import test_environment

def _ExtractTests(suite):
  """Returns all the tests from a given test suite."""
  tests = []
  for x in suite:
    if isinstance(x, unittest.TestSuite):
      tests += _ExtractTests(x)
    else:
      tests += [x]
  return tests


def _GetTestName(test):
  return '.'.join([test.__module__, test.__class__.__name__,
                  test._testMethodName])


def _FilterTests(all_tests, gtest_filter):
  """Returns a filtered list of tests based on the given gtest filter.

  See http://code.google.com/p/googletest/wiki/AdvancedGuide
  for gtest_filter specification.
  """
  pattern_groups = gtest_filter.split('-')
  positive_patterns = pattern_groups[0].split(':')
  negative_patterns = None
  if len(pattern_groups) > 1:
    negative_patterns = pattern_groups[1].split(':')

  tests = []
  for test in all_tests:
    test_name = _GetTestName(test)
    # Test name must by matched by one positive pattern.
    for pattern in positive_patterns:
      if fnmatch.fnmatch(test_name, pattern):
        break
    else:
      continue
    # Test name must not be matched by any negative patterns.
    for pattern in negative_patterns or []:
      if fnmatch.fnmatch(test_name, pattern):
        break
    else:
      tests += [test]
  return tests


if __name__ == '__main__':
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--filter', type='string', default='*',
      help='Filter for specifying what tests to run, google test style.')
  parser.add_option(
      '', '--driver-exe', type='string', default=None,
      help='Path to an alternative ChromeDriver executable.')
  parser.add_option(
      '', '--list', action='store_true', default=False,
      help='List tests instead of running them.')
  options, args = parser.parse_args()

  all_tests_suite = unittest.defaultTestLoader.loadTestsFromModule(
      chromedriver_tests)
  all_tests = _ExtractTests(all_tests_suite)
  filtered_tests = _FilterTests(all_tests, options.filter)

  if options.list is True:
    for test in filtered_tests:
      print _GetTestName(test)
    sys.exit(0)
  if sys.platform.startswith('darwin'):
    print 'All tests temporarily disabled on mac, crbug.com/103434'
    sys.exit(0)

  test_environment.SetUp(options.driver_exe)
  result = GTestTextTestRunner(verbosity=1).run(
      unittest.TestSuite(filtered_tests))
  test_environment.TearDown()
  sys.exit(not result.wasSuccessful())
