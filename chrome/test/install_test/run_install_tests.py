#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs install and update tests.

Install tests are performed using a single Chrome build, whereas two or more
builds are needed for Update tests. There are separate command arguments for
the builds that will be used for each of the tests. If a test file contains
both types of tests(Install and Update), both arguments should be specified.
Otherwise, specify only the command argument that is required for the test.
This script can be used as an executable to run other Install/Upgrade tests.

Example:
    $ python run_install_test.py <script_name> --url=<chrome_builds_url> \
      --install-build=24.0.1290.0 --update-builds=24.0.1289.0,24.0.1290.0
"""

import fnmatch
import logging
import optparse
import os
import re
import sys
import unittest

import chrome_installer_win
from install_test import InstallTest
import sample_updater

_DIRECTORY = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(os.path.dirname(_DIRECTORY), 'pyautolib'))

import pyauto_utils


class Main(object):
  """Main program for running 'Fresh Install' and 'Updater' tests."""

  def __init__(self):
    self._SetLoggingConfiguration()
    self._ParseArgs()
    self._Run()

  def _ParseArgs(self):
    """Parses command line arguments."""
    parser = optparse.OptionParser()
    parser.add_option(
        '-u', '--url', type='string', default='', dest='url',
        help='Specifies the build url, without the build number.')
    parser.add_option(
        '-o', '--options', type='string', default='',
        help='Specifies any additional Chrome options (i.e. --system-level).')
    parser.add_option(
        '--install-build', type='string', default='', dest='install_build',
        help='Specifies the build to be used for fresh install testing.')
    parser.add_option(
        '--update-builds', type='string', default='', dest='update_builds',
        help='Specifies the builds to be used for updater testing.')
    parser.add_option(
        '--install-type', type='string', default='user', dest='install_type',
        help='Type of installation (i.e., user, system, or both)')
    parser.add_option(
        '-f', '--filter', type='string', default='*', dest='filter',
        help='Filter that specifies the test or testsuite to run.')
    self._opts, self._args = parser.parse_args()
    self._ValidateArgs()
    if(self._opts.install_type == 'system' or
       self._opts.install_type == 'user'):
      install_type = ({
          'system' : chrome_installer_win.InstallationType.SYSTEM,
          'user' : chrome_installer_win.InstallationType.USER}).get(
              self._opts.install_type)
      InstallTest.SetInstallType(install_type)
    update_builds = (self._opts.update_builds.split(',') if
                     self._opts.update_builds else [])
    options = self._opts.options.split(',') if self._opts.options else []
    InstallTest.InitTestFixture(self._opts.install_build, update_builds,
                                self._opts.url, options)

  def _ValidateArgs(self):
    """Verifies the sanity of the command arguments.

    Confirms that all specified builds have a valid version number, and the
    build urls are valid.
    """
    builds = []
    if self._opts.install_build:
      builds.append(self._opts.install_build)
    if self._opts.update_builds:
      builds.extend(self._opts.update_builds.split(','))
    builds = list(frozenset(builds))
    for build in builds:
      if not re.match('\d+\.\d+\.\d+\.\d+', build):
        raise RuntimeError('Invalid build number: %s' % build)
      if not pyauto_utils.DoesUrlExist('%s/%s/' % (self._opts.url, build)):
        raise RuntimeError('Could not locate build no. %s' % build)

  def _SetLoggingConfiguration(self):
    """Sets the basic logging configuration."""
    log_format = '%(asctime)s %(levelname)-8s %(message)s'
    logging.basicConfig(level=logging.INFO, format=log_format)

  def _GetTestsFromSuite(self, suite):
    """Returns all the tests from a given test suite.

    Args:
      suite: A unittest.TestSuite object.

    Returns:
      A list that contains all the tests found in the suite.
    """
    tests = []
    for test in suite:
      if isinstance(test, unittest.TestSuite):
        tests += self._GetTestsFromSuite(test)
      else:
        tests += [test]
    return tests

  def _GetTestName(self, test):
    """Gets the test name of the given unittest test.

    Args:
      test: A unittest test.

    Returns:
      A string representing the full test name.
    """
    return '.'.join([test.__module__, test.__class__.__name__,
                     test._testMethodName])

  def _FilterTestSuite(self, suite, gtest_filter):
    """Returns a new filtered test suite based on the given gtest filter.

    See http://code.google.com/p/googletest/wiki/AdvancedGuide for
    gtest_filter specification.

    Args:
      suite: A unittest.TestSuite object, which can be obtained by calling
             |unittest.defaultTestLoader.loadTestsFromName|.
      gtest_filter: The gtest filter to use. Filter can be passed as follows:
                    --filter=*className* or --filter=*testcaseName.

    Returns:
      A unittest.TestSuite object that contains tests that match the gtest
      filter.
    """
    return unittest.TestSuite(
        self._FilterTests(self._GetTestsFromSuite(suite), gtest_filter))

  def _FilterTests(self, all_tests, gtest_filter):
    """Returns a filtered list of tests based on the given gtest filter.

    Args:
      all_tests: A list that contains all unittests in a given suite. This
                 list must be obtained by calling |_GetTestsFromSuite|.
      gtest_filter: The gtest filter to use. Filter can be passed as follows:
                    *className* or *testcaseName.

    Returns:
      A list that contains all tests that match the given gtest filter.
    """
    pattern_groups = gtest_filter.split('-')
    positive_patterns = pattern_groups[0].split(':')
    negative_patterns = None
    if len(pattern_groups) > 1:
      negative_patterns = pattern_groups[1].split(':')
    tests = []
    for test in all_tests:
      test_name = self._GetTestName(test)
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

  def _Run(self):
    """Runs the unit tests."""
    all_tests = unittest.defaultTestLoader.loadTestsFromModule(sample_updater)
    tests = self._FilterTestSuite(all_tests, self._opts.filter)
    result = pyauto_utils.GTestTextTestRunner(verbosity=1).run(tests)
    del(tests)
    if not result.wasSuccessful():
      print >>sys.stderr, ('Not all tests were successful.')
      sys.exit(1)
    sys.exit(0)


if __name__ == '__main__':
  Main()
