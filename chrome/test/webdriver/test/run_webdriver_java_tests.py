#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs the WebDriver Java acceptance tests.

This script is called from buildbot and reports results using the buildbot
annotation scheme.

For ChromeDriver documentation, refer to http://code.google.com/p/chromedriver.
"""

import optparse
import os
import sys

import continuous_archive
import java_tests
import util


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--filter', type='string', default=None,
      help='Filter for specifying what tests to run. E.g., MyClass#testMethod.')
  parser.add_option(
      '', '--bot', type='string', default=None,
      help='Executes the tests for the specified bot type. E.g., fyi')
  options, args = parser.parse_args()

  if 'WEBDRIVER_CHECKOUT' not in os.environ:
    raise RuntimeError('WEBDRIVER_CHECKOUT must be defined in the environment')
  webdriver_checkout = os.environ['WEBDRIVER_CHECKOUT']

  if options.bot == 'fyi':
    return RunFyiTests(options, webdriver_checkout)
  else:
    raise RuntimeError('Unrecognized bot: ' + options.bot)


def RunFyiTests(options, webdriver_checkout):
  """Runs tests for the FYI buildbot configuration."""
  print '@@@BUILD_STEP java_continuous_tests@@@'
  revision = continuous_archive.GetLatestRevision()
  print '@@@STEP_TEXT@r%s@@@' % revision
  temp_dir = util.MakeTempDir()

  chrome_path = continuous_archive.DownloadChrome(revision, temp_dir)
  chromedriver_path = continuous_archive.DownloadChromeDriver(
      revision, temp_dir)

  PrintTestResults(java_tests.Run(
      test_target=java_tests.CHROME_TESTS,
      test_filter=options.filter,
      webdriver_dir=webdriver_checkout,
      chromedriver_path=chromedriver_path,
      chrome_path=chrome_path))

  # Disable running latest ChromeDriver with Chrome stable, since
  # there is a compatibility break between v20 and v21.
  # See http://code.google.com/p/chromedriver/issues/detail?id=80
  # print '@@@BUILD_STEP java_stable_tests@@@'
  # print '@@@STEP_TEXT@chromedriver r%s@@@' % revision
  # PrintTestResults(java_tests.Run(
  #     test_target=java_tests.CHROME_TESTS,
  #     test_filter=options.filter,
  #     webdriver_dir=webdriver_checkout,
  #     chromedriver_path=chromedriver_path,
  #     chrome_path=None))


def PrintTestResults(results):
  """Prints the given results in a format recognized by the buildbot."""
  failures = []
  for result in results:
    if not result.IsPass():
      failures += [result]

  print 'Ran %s tests' % len(results)
  print 'Failed %s:' % len(failures)
  for result in failures:
    print '=' * 80
    print '=' * 10, result.GetName(), '(%ss)' % result.GetTime()
    print result.GetFailureMessage()
  if len(failures) > 0:
    print '@@@STEP_TEXT@Failed %s tests@@@' % len(failures)
    print '@@@STEP_FAILURE@@@'


if __name__ == '__main__':
  sys.exit(main())
