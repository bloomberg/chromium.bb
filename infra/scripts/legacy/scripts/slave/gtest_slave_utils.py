#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import optparse
import os
import re
import sys

from common import gtest_utils
from xml.dom import minidom
from slave.gtest.json_results_generator import JSONResultsGenerator
from slave.gtest.test_result import canonical_name
from slave.gtest.test_result import TestResult


GENERATE_JSON_RESULTS_OPTIONS = [
    'builder_name', 'build_name', 'build_number', 'results_directory',
    'builder_base_url', 'webkit_revision', 'chrome_revision',
    'test_results_server', 'test_type', 'master_name']

FULL_RESULTS_FILENAME = 'full_results.json'
TIMES_MS_FILENAME = 'times_ms.json'


# Note: GTestUnexpectedDeathTracker is being deprecated in favor of
# common.gtest_utils.GTestLogParser. See scripts/slave/runtest.py for details.
class GTestUnexpectedDeathTracker(object):
  """A lightweight version of log parser that keeps track of running tests
  for unexpected timeout or crash."""

  def __init__(self):
    self._current_test = None
    self._completed = False
    self._test_start = re.compile(r'\[\s+RUN\s+\] (\w+\.\w+)')
    self._test_ok = re.compile(r'\[\s+OK\s+\] (\w+\.\w+)')
    self._test_fail = re.compile(r'\[\s+FAILED\s+\] (\w+\.\w+)')
    self._test_passed = re.compile(r'\[\s+PASSED\s+\] \d+ tests?.')

    self._failed_tests = set()

  def OnReceiveLine(self, line):
    results = self._test_start.search(line)
    if results:
      self._current_test = results.group(1)
      return

    results = self._test_ok.search(line)
    if results:
      self._current_test = ''
      return

    results = self._test_fail.search(line)
    if results:
      self._failed_tests.add(results.group(1))
      self._current_test = ''
      return

    results = self._test_passed.search(line)
    if results:
      self._completed = True
      self._current_test = ''
      return

  def GetResultsMap(self):
    """Returns a map of TestResults."""

    if self._current_test:
      self._failed_tests.add(self._current_test)

    test_results_map = dict()
    for test in self._failed_tests:
      test_results_map[canonical_name(test)] = [TestResult(test, failed=True)]

    return test_results_map

  def CompletedWithoutFailure(self):
    """Returns True if all tests completed and no tests failed unexpectedly."""

    if not self._completed:
      return False

    for test in self._failed_tests:
      test_modifier = TestResult(test, failed=True).modifier
      if test_modifier not in (TestResult.FAILS, TestResult.FLAKY):
        return False

    return True


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


def GetResultsMapFromXML(results_xml):
  """Parse the given results XML file and returns a map of TestResults."""

  results_xml_file = None
  try:
    results_xml_file = open(results_xml)
  except IOError:
    logging.error('Cannot open file %s', results_xml)
    return dict()
  node = minidom.parse(results_xml_file).documentElement
  results_xml_file.close()

  test_results_map = dict()
  testcases = node.getElementsByTagName('testcase')

  for testcase in testcases:
    name = testcase.getAttribute('name')
    classname = testcase.getAttribute('classname')
    test_name = '%s.%s' % (classname, name)

    failures = testcase.getElementsByTagName('failure')
    not_run = testcase.getAttribute('status') == 'notrun'
    elapsed = float(testcase.getAttribute('time'))
    result = TestResult(test_name,
                        failed=bool(failures),
                        not_run=not_run,
                        elapsed_time=elapsed)
    test_results_map[canonical_name(test_name)] = [result]

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

# For command-line testing.
def main():
  # Builder base URL where we have the archived test results.
  # (Note: to be deprecated)
  BUILDER_BASE_URL = 'http://build.chromium.org/buildbot/gtest_results/'

  option_parser = optparse.OptionParser()
  option_parser.add_option('', '--test-type', default='',
                           help='Test type that generated the results XML,'
                                ' e.g. unit-tests.')
  option_parser.add_option('', '--results-directory', default='./',
                           help='Output results directory source dir.')
  option_parser.add_option('', '--input-results-xml', default='',
                           help='Test results xml file (input for us).'
                                ' default is TEST_TYPE.xml')
  option_parser.add_option('', '--builder-base-url', default='',
                           help=('A URL where we have the archived test '
                                 'results. (default=%sTEST_TYPE_results/)'
                                 % BUILDER_BASE_URL))
  option_parser.add_option('', '--builder-name',
                           default='DUMMY_BUILDER_NAME',
                           help='The name of the builder shown on the '
                                'waterfall running this script e.g. WebKit.')
  option_parser.add_option('', '--build-name',
                           default='DUMMY_BUILD_NAME',
                           help='The name of the builder used in its path, '
                                'e.g. webkit-rel.')
  option_parser.add_option('', '--build-number', default='',
                           help='The build number of the builder running'
                                'this script.')
  option_parser.add_option('', '--test-results-server',
                           default='',
                           help='The test results server to upload the '
                                'results.')
  option_parser.add_option('--master-name', default='',
                           help='The name of the buildbot master. '
                                'Both test-results-server and master-name '
                                'need to be specified to upload the results '
                                'to the server.')
  option_parser.add_option('--webkit-revision', default='0',
                           help='The WebKit revision being tested. If not '
                                'given, defaults to 0.')
  option_parser.add_option('--chrome-revision', default='0',
                           help='The Chromium revision being tested. If not '
                                'given, defaults to 0.')

  options = option_parser.parse_args()[0]

  if not options.test_type:
    logging.error('--test-type needs to be specified.')
    sys.exit(1)

  if not options.input_results_xml:
    logging.error('--input-results-xml needs to be specified.')
    sys.exit(1)

  if options.test_results_server and not options.master_name:
    logging.warn('--test-results-server is given but '
                 '--master-name is not specified; the results won\'t be '
                 'uploaded to the server.')

  results_map = GetResultsMapFromXML(options.input_results_xml)
  generator = GenerateJSONResults(results_map, options)
  UploadJSONResults(generator)


if '__main__' == __name__:
  main()
