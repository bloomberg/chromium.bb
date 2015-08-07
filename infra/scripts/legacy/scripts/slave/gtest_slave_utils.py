#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from common import gtest_utils
from slave.gtest.json_results_generator import JSONResultsGenerator
from slave.gtest.test_result import canonical_name
from slave.gtest.test_result import TestResult


GENERATE_JSON_RESULTS_OPTIONS = [
    'builder_name', 'build_name', 'build_number', 'results_directory',
    'builder_base_url', 'webkit_revision', 'chrome_revision',
    'test_results_server', 'test_type', 'master_name']

FULL_RESULTS_FILENAME = 'full_results.json'
TIMES_MS_FILENAME = 'times_ms.json'


def GetResultsMap(observer):
  """Returns a map of TestResults."""

  test_results_map = dict()
  tests = (observer.FailedTests(include_fails=True, include_flaky=True) +
           observer.PassedTests())
  for test in tests:
    key = canonical_name(test)
    test_results_map[key] = []
    tries = observer.TriesForTest(test)
    for test_try in tries:
      # FIXME: Store the actual failure type so we can expose whether the test
      # crashed or timed out. See crbug.com/249965.
      failed = (test_try != gtest_utils.TEST_SUCCESS_LABEL)
      test_results_map[key].append(TestResult(test, failed=failed))

  return test_results_map


def GenerateJSONResults(test_results_map, options):
  """Generates a JSON results file from the given test_results_map,
  returning the associated generator for use with UploadJSONResults, below.

  Args:
    test_results_map: A map of TestResult.
    options: options for json generation. See GENERATE_JSON_RESULTS_OPTIONS
        and OptionParser's help messages below for expected options and their
        details.
  """

  if not test_results_map:
    logging.warn('No input results map was given.')
    return

  # Make sure we have all the required options (set empty string otherwise).
  for opt in GENERATE_JSON_RESULTS_OPTIONS:
    if not getattr(options, opt, None):
      logging.warn('No value is given for option %s', opt)
      setattr(options, opt, '')

  try:
    int(options.build_number)
  except ValueError:
    logging.error('options.build_number needs to be a number: %s',
                  options.build_number)
    return

  if not os.path.exists(options.results_directory):
    os.makedirs(options.results_directory)

  print('Generating json: '
        'builder_name:%s, build_name:%s, build_number:%s, '
        'results_directory:%s, builder_base_url:%s, '
        'webkit_revision:%s, chrome_revision:%s '
        'test_results_server:%s, test_type:%s, master_name:%s' %
        (options.builder_name, options.build_name, options.build_number,
         options.results_directory, options.builder_base_url,
         options.webkit_revision, options.chrome_revision,
         options.test_results_server, options.test_type,
         options.master_name))

  generator = JSONResultsGenerator(
      options.builder_name, options.build_name, options.build_number,
      options.results_directory, options.builder_base_url,
      test_results_map,
      svn_revisions=(('blink', options.webkit_revision),
                     ('chromium', options.chrome_revision)),
      test_results_server=options.test_results_server,
      test_type=options.test_type,
      master_name=options.master_name)
  generator.generate_json_output()
  generator.generate_times_ms_file()
  return generator

def UploadJSONResults(generator):
  """Conditionally uploads the results from GenerateJSONResults if
  test_results_server was given."""
  if generator:
    generator.upload_json_files([FULL_RESULTS_FILENAME,
                                 TIMES_MS_FILENAME])
