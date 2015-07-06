#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from common import chromium_utils
import json
import os
import re
import tempfile


# These labels should match the ones output by gtest's JSON.
TEST_UNKNOWN_LABEL = 'UNKNOWN'
TEST_SUCCESS_LABEL = 'SUCCESS'
TEST_FAILURE_LABEL = 'FAILURE'
TEST_FAILURE_ON_EXIT_LABEL = 'FAILURE_ON_EXIT'
TEST_CRASH_LABEL = 'CRASH'
TEST_TIMEOUT_LABEL = 'TIMEOUT'
TEST_SKIPPED_LABEL = 'SKIPPED'
TEST_WARNING_LABEL = 'WARNING'

FULL_RESULTS_FILENAME = 'full_results.json'
TIMES_MS_FILENAME = 'times_ms.json'

def CompressList(lines, max_length, middle_replacement):
  """Ensures that |lines| is no longer than |max_length|. If |lines| need to
  be compressed then the middle items are replaced by |middle_replacement|.
  """
  if len(lines) <= max_length:
    return lines
  remove_from_start = max_length / 2
  return (lines[:remove_from_start] +
          [middle_replacement] +
          lines[len(lines) - (max_length - remove_from_start):])


class GTestJSONParser(object):
  # Limit of output snippet lines. Avoids flooding the logs with amount
  # of output that gums up the infrastructure.
  OUTPUT_SNIPPET_LINES_LIMIT = 5000

  def __init__(self, mastername=None):
    self.json_file_path = None
    self.delete_json_file = False

    self.disabled_tests = set()
    self.passed_tests = set()
    self.failed_tests = set()
    self.flaky_tests = set()
    self.test_logs = {}
    self.run_results = {}
    self.ignored_failed_tests = set()

    self.parsing_errors = []

    self.master_name = mastername

    # List our labels that match the ones output by gtest JSON.
    self.SUPPORTED_LABELS = (TEST_UNKNOWN_LABEL,
                             TEST_SUCCESS_LABEL,
                             TEST_FAILURE_LABEL,
                             TEST_FAILURE_ON_EXIT_LABEL,
                             TEST_CRASH_LABEL,
                             TEST_TIMEOUT_LABEL,
                             TEST_SKIPPED_LABEL)

  def ProcessLine(self, line):
    # Deliberately do nothing - we parse out-of-band JSON summary
    # instead of in-band stdout.
    pass

  def PassedTests(self):
    return sorted(self.passed_tests)

  def FailedTests(self, include_fails=False, include_flaky=False):
    return sorted(self.failed_tests - self.ignored_failed_tests)

  def TriesForTest(self, test):
    """Returns a list containing the state for all tries of the given test."""
    return self.run_results.get(test, [TEST_UNKNOWN_LABEL])

  def FailureDescription(self, test):
    return self.test_logs.get(test, [])

  def IgnoredFailedTests(self):
    return sorted(self.ignored_failed_tests)

  @staticmethod
  def MemoryToolReportHashes():
    return []

  def ParsingErrors(self):
    return self.parsing_errors

  def ClearParsingErrors(self):
    self.parsing_errors = ['Cleared.']

  def DisabledTests(self):
    return len(self.disabled_tests)

  def FlakyTests(self):
    return len(self.flaky_tests)

  @staticmethod
  def RunningTests():
    return []

  def PrepareJSONFile(self, cmdline_path):
    if cmdline_path:
      self.json_file_path = cmdline_path
      # If the caller requested JSON summary, do not delete it.
      self.delete_json_file = False
    else:
      fd, self.json_file_path = tempfile.mkstemp()
      os.close(fd)
      # When we create the file ourselves, delete it to avoid littering.
      self.delete_json_file = True
    return self.json_file_path

  def ProcessJSONFile(self, build_dir):
    if not self.json_file_path:
      return

    with open(self.json_file_path) as json_file:
      try:
        json_output = json_file.read()
        json_data = json.loads(json_output)
      except ValueError:
        # Only signal parsing error if the file is non-empty. Empty file
        # most likely means the binary doesn't support JSON output.
        if json_output:
          self.parsing_errors = json_output.split('\n')
      else:
        self.ProcessJSONData(json_data, build_dir)

    if self.delete_json_file:
      os.remove(self.json_file_path)

  @staticmethod
  def ParseIgnoredFailedTestSpec(dir_in_chrome):
    """Returns parsed ignored failed test spec.

    Args:
      dir_in_chrome: Any directory within chrome checkout to be used as a
                     reference to find ignored failed test spec file.

    Returns:
      A list of tuples (test_name, platforms), where platforms is a list of sets
      of platform flags. For example:

        [('MyTest.TestOne', [set('OS_WIN', 'CPU_32_BITS', 'MODE_RELEASE'),
                             set('OS_LINUX', 'CPU_64_BITS', 'MODE_DEBUG')]),
         ('MyTest.TestTwo', [set('OS_MACOSX', 'CPU_64_BITS', 'MODE_RELEASE'),
                             set('CPU_32_BITS')]),
         ('MyTest.TestThree', [set()]]
    """

    try:
      ignored_failed_tests_path = chromium_utils.FindUpward(
          os.path.abspath(dir_in_chrome), 'tools', 'ignorer_bot',
          'ignored_failed_tests.txt')
    except chromium_utils.PathNotFound:
      return

    with open(ignored_failed_tests_path) as ignored_failed_tests_file:
      ignored_failed_tests_spec = ignored_failed_tests_file.readlines()

    parsed_spec = []
    for spec_line in ignored_failed_tests_spec:
      spec_line = spec_line.strip()
      if spec_line.startswith('#') or not spec_line:
        continue

      # Any number of platform flags identifiers separated by whitespace.
      platform_spec_regexp = r'[A-Za-z0-9_\s]*'

      match = re.match(
          r'^crbug.com/\d+'           # Issue URL.
          r'\s+'                      # Some whitespace.
          r'\[(' +                    # Opening square bracket '['.
            platform_spec_regexp +    # At least one platform, and...
            r'(?:,' +                 # ...separated by commas...
              platform_spec_regexp +  # ...any number of additional...
            r')*'                     # ...platforms.
          r')\]'                      # Closing square bracket ']'.
          r'\s+'                      # Some whitespace.
          r'(\S+)$', spec_line)       # Test name.

      if not match:
        continue

      platform_specs = match.group(1).strip()
      test_name = match.group(2).strip()

      platforms = [set(platform.split())
                   for platform in platform_specs.split(',')]

      parsed_spec.append((test_name, platforms))

    return parsed_spec


  def _RetrieveIgnoredFailuresForPlatform(self, build_dir, platform_flags):
    """Parses the ignored failed tests spec into self.ignored_failed_tests."""
    if not build_dir:
      return

    platform_flags = set(platform_flags)
    parsed_spec = self.ParseIgnoredFailedTestSpec(build_dir)

    if not parsed_spec:
      return

    for test_name, platforms in parsed_spec:
      for required_platform_flags in platforms:
        if required_platform_flags.issubset(platform_flags):
          self.ignored_failed_tests.add(test_name)
          break

  def ProcessJSONData(self, json_data, build_dir=None):
    self.disabled_tests.update(json_data['disabled_tests'])
    self._RetrieveIgnoredFailuresForPlatform(
        build_dir, json_data['global_tags'])

    for iteration_data in json_data['per_iteration_data']:
      for test_name, test_runs in iteration_data.iteritems():
        if test_runs[-1]['status'] == 'SUCCESS':
          self.passed_tests.add(test_name)
        else:
          self.failed_tests.add(test_name)

        self.run_results[test_name] = []
        self.test_logs.setdefault(test_name, [])
        for run_index, run_data in enumerate(test_runs, start=1):
          # Mark as flaky if the run result differs.
          if run_data['status'] != test_runs[0]['status']:
            self.flaky_tests.add(test_name)
          if run_data['status'] in self.SUPPORTED_LABELS:
            self.run_results[test_name].append(run_data['status'])
          else:
            self.run_results[test_name].append(TEST_UNKNOWN_LABEL)
          run_lines = ['%s (run #%d):' % (test_name, run_index)]
          # Make sure the annotations are ASCII to avoid character set related
          # errors. They are mostly informational anyway, and more detailed
          # info can be obtained from the original JSON output.
          ascii_lines = run_data['output_snippet'].encode('ascii',
                                                          errors='replace')
          decoded_lines = CompressList(
              ascii_lines.decode('string_escape').split('\n'),
              self.OUTPUT_SNIPPET_LINES_LIMIT,
              '<truncated, full output is in gzipped JSON '
              'output at end of step>')
          run_lines.extend(decoded_lines)
          self.test_logs[test_name].extend(run_lines)
