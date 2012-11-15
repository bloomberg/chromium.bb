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

sys.path.insert(0, os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, 'pylib'))

from common import chrome_paths
from common import util
import continuous_archive
import java_tests


def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '', '--filter', type='string', default=None,
      help='Filter for specifying what tests to run. E.g., ' +
           'ElementFindingTest#testShouldReturnTitleOfPageIfSet.')
  options, args = parser.parse_args()

  print '@@@BUILD_STEP java_continuous_tests@@@'
  # We use the latest revision in the continuous archive instead of the
  # extracted build for two reasons:
  # 1) Builds are only archived if they have passed some set of tests. This
  #    results in less false failures.
  # 2) I don't want to add chromedriver to the chromium_builder_tests target in
  #    all.gyp, since that will probably add a minute or so to every build.
  revision = continuous_archive.GetLatestRevision()
  print '@@@STEP_TEXT@r%s@@@' % revision
  temp_dir = util.MakeTempDir()

  chrome_path = continuous_archive.DownloadChrome(revision, temp_dir)
  chromedriver_path = continuous_archive.DownloadChromeDriver(
      revision, temp_dir)

  PrintTestResults(java_tests.Run(
      src_dir=chrome_paths.GetSrc(),
      test_filter=options.filter,
      chromedriver_path=chromedriver_path,
      chrome_path=chrome_path))

  print '@@@BUILD_STEP java_stable_tests@@@'
  print '@@@STEP_TEXT@chromedriver r%s@@@' % revision
  PrintTestResults(java_tests.Run(
      src_dir=chrome_paths.GetSrc(),
      test_filter=options.filter,
      chromedriver_path=chromedriver_path,
      chrome_path=None))


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
