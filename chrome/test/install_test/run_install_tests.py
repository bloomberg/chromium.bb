#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs install and update tests.

Install tests are performed using a single Chrome build, whereas two or more
builds are needed for Update tests. There are separate command arguments for
the builds that will be used for each of the tests. If a test file contains
both types of tests(install and update), both arguments should be specified.
Otherwise, specify only the command argument that is required for the test.
To run a test with this script, append the module name to the _TEST_MODULES
list. Modules added to the list must be in the same directory or in a sub-
directory that's in the same location as this script.

Example:
    $ python run_install_tests.py --url=<chrome_builds_url> --filter=* \
      --install-build=24.0.1290.0 --update-builds=24.0.1289.0,24.0.1290.0
"""

import logging
import optparse
import os
import re
import sys
import unittest

import chrome_installer_win
from install_test import InstallTest

from common import unittest_util
from common import util

# To run tests from a module, append the module name to this list.
_TEST_MODULES = ['sample_updater', 'theme_updater']

for module in _TEST_MODULES:
  __import__(module)


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
    if self._opts.install_type == 'system':
      InstallTest.SetInstallType(chrome_installer_win.InstallationType.SYSTEM)
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
      if not util.DoesUrlExist('%s/%s/' % (self._opts.url, build)):
        raise RuntimeError('Could not locate build no. %s' % build)

  def _SetLoggingConfiguration(self):
    """Sets the basic logging configuration."""
    log_format = '%(asctime)s %(levelname)-8s %(message)s'
    logging.basicConfig(level=logging.INFO, format=log_format)

  def _Run(self):
    """Runs the unit tests."""
    all_tests = unittest.defaultTestLoader.loadTestsFromNames(_TEST_MODULES)
    tests = unittest_util.FilterTestSuite(all_tests, self._opts.filter)
    result = unittest_util.TextTestRunner(verbosity=1).run(tests)
    # Run tests again if installation type is 'both'(i.e., user and system).
    if self._opts.install_type == 'both':
      # Load the tests again so test parameters can be reinitialized.
      all_tests = unittest.defaultTestLoader.loadTestsFromNames(_TEST_MODULES)
      tests = unittest_util.FilterTestSuite(all_tests, self._opts.filter)
      InstallTest.SetInstallType(chrome_installer_win.InstallationType.SYSTEM)
      result = unittest_util.TextTestRunner(verbosity=1).run(tests)
    del(tests)
    if not result.wasSuccessful():
      print >>sys.stderr, ('Not all tests were successful.')
      sys.exit(1)
    sys.exit(0)


if __name__ == '__main__':
  Main()
