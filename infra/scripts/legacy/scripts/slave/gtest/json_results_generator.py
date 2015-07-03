# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A utility class to generate JSON results from given test results and upload
them to the specified results server.

"""

from __future__ import with_statement

import codecs
import logging
import os
import time

import simplejson
from slave.gtest.test_result import TestResult
from slave.gtest.test_results_uploader import TestResultsUploader

# A JSON results generator for generic tests.

JSON_PREFIX = 'ADD_RESULTS('
JSON_SUFFIX = ');'


def test_did_pass(test_result):
  return not test_result.failed and test_result.modifier == TestResult.NONE


def add_path_to_trie(path, value, trie):
  """Inserts a single flat directory path and associated value into a directory
  trie structure."""
  if not '/' in path:
    trie[path] = value
    return

  # we don't use slash
  # pylint: disable=W0612
  directory, slash, rest = path.partition('/')
  if not directory in trie:
    trie[directory] = {}
  add_path_to_trie(rest, value, trie[directory])


def generate_test_timings_trie(individual_test_timings):
  """Breaks a test name into chunks by directory and puts the test time as a
  value in the lowest part, e.g.
  foo/bar/baz.html: 1ms
  foo/bar/baz1.html: 3ms

  becomes
  foo: {
    bar: {
      baz.html: 1,
      baz1.html: 3
    }
  }
  """
  trie = {}
  # Only use the timing of the first try of each test.
  for test_results in individual_test_timings:
    test = test_results[0].test_name

    add_path_to_trie(test, int(1000 * test_results[-1].test_run_time), trie)

  return trie


class JSONResultsGenerator(object):
  """A JSON results generator for generic tests."""

  FAIL_LABEL = 'FAIL'
  PASS_LABEL = 'PASS'
  FLAKY_LABEL = ' '.join([FAIL_LABEL, PASS_LABEL])
  SKIP_LABEL = 'SKIP'

  ACTUAL = 'actual'
  BLINK_REVISION = 'blink_revision'
  BUILD_NUMBER = 'build_number'
  BUILDER_NAME = 'builder_name'
  CHROMIUM_REVISION = 'chromium_revision'
  EXPECTED = 'expected'
  FAILURE_SUMMARY = 'num_failures_by_type'
  SECONDS_SINCE_EPOCH = 'seconds_since_epoch'
  TEST_TIME = 'time'
  TESTS = 'tests'
  VERSION = 'version'
  VERSION_NUMBER = 3

  RESULTS_FILENAME = 'results.json'
  TIMES_MS_FILENAME = 'times_ms.json'
  FULL_RESULTS_FILENAME = 'full_results.json'

  def __init__(self, builder_name, build_name, build_number,
               results_file_base_path, builder_base_url,
               test_results_map, svn_revisions=None,
               test_results_server=None,
               test_type='',
               master_name='',
               file_writer=None):
    """Modifies the results.json file. Grabs it off the archive directory
    if it is not found locally.

    Args
      builder_name: the builder name (e.g. Webkit).
      build_name: the build name (e.g. webkit-rel).
      build_number: the build number.
      results_file_base_path: Absolute path to the directory containing the
        results json file.
      builder_base_url: the URL where we have the archived test results.
        If this is None no archived results will be retrieved.
      test_results_map: A dictionary that maps test_name to a list of
        TestResult, one for each time the test was retried.
      svn_revisions: A (json_field_name, revision) pair for SVN
        repositories that tests rely on.  The SVN revision will be
        included in the JSON with the given json_field_name.
      test_results_server: server that hosts test results json.
      test_type: test type string (e.g. 'layout-tests').
      master_name: the name of the buildbot master.
      file_writer: if given the parameter is used to write JSON data to a file.
        The parameter must be the function that takes two arguments, 'file_path'
        and 'data' to be written into the file_path.
    """
    self._builder_name = builder_name
    self._build_name = build_name
    self._build_number = build_number
    self._builder_base_url = builder_base_url
    self._results_directory = results_file_base_path

    self._test_results_map = test_results_map

    self._svn_revisions = svn_revisions
    if not self._svn_revisions:
      self._svn_revisions = {}

    self._test_results_server = test_results_server
    self._test_type = test_type
    self._master_name = master_name
    self._file_writer = file_writer

  def generate_json_output(self):
    json = self.get_full_results_json()
    if json:
      file_path = os.path.join(self._results_directory,
                               self.FULL_RESULTS_FILENAME)
      self._write_json(json, file_path)

  def generate_times_ms_file(self):
    times = generate_test_timings_trie(self._test_results_map.values())
    file_path = os.path.join(self._results_directory, self.TIMES_MS_FILENAME)
    self._write_json(times, file_path)

  def get_full_results_json(self):
    results = {self.VERSION: self.VERSION_NUMBER}

    # Metadata generic to all results.
    results[self.BUILDER_NAME] = self._builder_name
    results[self.BUILD_NUMBER] = self._build_number
    results[self.SECONDS_SINCE_EPOCH] = int(time.time())
    for name, revision in self._svn_revisions:
      results[name + '_revision'] = revision

    tests = results.setdefault(self.TESTS, {})
    for test_name in self._test_results_map.iterkeys():
      tests[test_name] = self._make_test_data(test_name)

    self._insert_failure_map(results)

    return results

  def _insert_failure_map(self, results):
    # FAIL, PASS, NOTRUN
    summary = {self.PASS_LABEL: 0, self.FAIL_LABEL: 0, self.SKIP_LABEL: 0}
    for test_results in self._test_results_map.itervalues():
      # Use the result of the first test for aggregate statistics. This may
      # count as failing a test that passed on retry, but it's a more useful
      # statistic and it's consistent with our other test harnesses.
      test_result = test_results[0]
      if test_did_pass(test_result):
        summary[self.PASS_LABEL] += 1
      elif test_result.modifier == TestResult.DISABLED:
        summary[self.SKIP_LABEL] += 1
      elif test_result.failed:
        summary[self.FAIL_LABEL] += 1

    results[self.FAILURE_SUMMARY] = summary

  def _make_test_data(self, test_name):
    test_data = {}
    expected, actual = self._get_expected_and_actual_results(test_name)
    test_data[self.EXPECTED] = expected
    test_data[self.ACTUAL] = actual
    # Use the timing of the first try, it's a better representative since it
    # runs under more load than retries.
    run_time = int(self._test_results_map[test_name][0].test_run_time)
    test_data[self.TEST_TIME] = run_time

    return test_data

  def _get_expected_and_actual_results(self, test_name):
    test_results = self._test_results_map[test_name]
    # Use the modifier of the first try, they should all be the same.
    modifier = test_results[0].modifier

    if modifier == TestResult.DISABLED:
      return (self.SKIP_LABEL, self.SKIP_LABEL)

    actual_list = []
    for test_result in test_results:
      label = self.FAIL_LABEL if test_result.failed else self.PASS_LABEL
      actual_list.append(label)
    actual = " ".join(actual_list)

    if modifier == TestResult.NONE:
      return (self.PASS_LABEL, actual)
    if modifier == TestResult.FLAKY:
      return (self.FLAKY_LABEL, actual)
    if modifier == TestResult.FAILS:
      return (self.FAIL_LABEL, actual)

  def upload_json_files(self, json_files):
    """Uploads the given json_files to the test_results_server (if the
    test_results_server is given)."""
    if not self._test_results_server:
      return

    if not self._master_name:
      logging.error('--test-results-server was set, but --master-name was not. '
                    'Not uploading JSON files.')
      return

    print 'Uploading JSON files for builder: %s' % self._builder_name
    attrs = [('builder', self._builder_name),
             ('testtype', self._test_type),
             ('master', self._master_name)]

    files = [(f, os.path.join(self._results_directory, f)) for f in json_files]

    uploader = TestResultsUploader(self._test_results_server)
    # Set uploading timeout in case appengine server is having problem.
    # 120 seconds are more than enough to upload test results.
    uploader.upload(attrs, files, 120)

    print 'JSON files uploaded.'

  def _write_json(self, json_object, file_path):
    # Specify separators in order to get compact encoding.
    json_data = simplejson.dumps(json_object, separators=(',', ':'))
    json_string = json_data
    if self._file_writer:
      self._file_writer(file_path, json_string)
    else:
      with codecs.open(file_path, 'w', 'utf8') as f:
        f.write(json_string)
