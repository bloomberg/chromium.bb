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


class GTestLogParser(object):
  """This helper class process GTest test output."""

  def __init__(self):
    # State tracking for log parsing
    self.completed = False
    self._current_test = ''
    self._failure_description = []
    self._current_report_hash = ''
    self._current_report = []
    self._parsing_failures = False

    # Line number currently being processed.
    self._line_number = 0

    # List of parsing errors, as human-readable strings.
    self._internal_error_lines = []

    # Tests are stored here as 'test.name': (status, [description]).
    # The status should be one of ('started', 'OK', 'failed', 'timeout',
    # 'warning'). Warning indicates that a test did not pass when run in
    # parallel with other tests but passed when run alone. The description is
    # a list of lines detailing the test's error, as reported in the log.
    self._test_status = {}

    # Reports are stored here as 'hash': [report].
    self._memory_tool_reports = {}

    # This may be either text or a number. It will be used in the phrase
    # '%s disabled' or '%s flaky' on the waterfall display.
    self._disabled_tests = 0
    self._flaky_tests = 0

    # Regular expressions for parsing GTest logs. Test names look like
    #   SomeTestCase.SomeTest
    #   SomeName/SomeTestCase.SomeTest/1
    # This regexp also matches SomeName.SomeTest/1, which should be harmless.
    test_name_regexp = r'((\w+/)?\w+\.\w+(/\d+)?)'

    self._master_name_re = re.compile(r'\[Running for master: "([^"]*)"')
    self.master_name = ''

    self._test_name = re.compile(test_name_regexp)
    self._test_start = re.compile(r'\[\s+RUN\s+\] ' + test_name_regexp)
    self._test_ok = re.compile(r'\[\s+OK\s+\] ' + test_name_regexp)
    self._test_fail = re.compile(r'\[\s+FAILED\s+\] ' + test_name_regexp)
    self._test_passed = re.compile(r'\[\s+PASSED\s+\] \d+ tests?.')
    self._run_test_cases_line = re.compile(
        r'\[\s*\d+\/\d+\]\s+[0-9\.]+s ' + test_name_regexp + ' .+')
    self._test_timeout = re.compile(
        r'Test timeout \([0-9]+ ms\) exceeded for ' + test_name_regexp)
    self._disabled = re.compile(r'\s*YOU HAVE (\d+) DISABLED TEST')
    self._flaky = re.compile(r'\s*YOU HAVE (\d+) FLAKY TEST')

    self._report_start = re.compile(
        r'### BEGIN MEMORY TOOL REPORT \(error hash=#([0-9A-F]+)#\)')
    self._report_end = re.compile(
        r'### END MEMORY TOOL REPORT \(error hash=#([0-9A-F]+)#\)')

    self._retry_message = re.compile('RETRYING FAILED TESTS:')
    self.retrying_failed = False

    self.TEST_STATUS_MAP = {
      'OK': TEST_SUCCESS_LABEL,
      'failed': TEST_FAILURE_LABEL,
      'timeout': TEST_TIMEOUT_LABEL,
      'warning': TEST_WARNING_LABEL
    }

  def GetCurrentTest(self):
    return self._current_test

  def _StatusOfTest(self, test):
    """Returns the status code for the given test, or 'not known'."""
    test_status = self._test_status.get(test, ('not known', []))
    return test_status[0]

  def _TestsByStatus(self, status, include_fails, include_flaky):
    """Returns list of tests with the given status.

    Args:
      include_fails: If False, tests containing 'FAILS_' anywhere in their
          names will be excluded from the list.
      include_flaky: If False, tests containing 'FLAKY_' anywhere in their
          names will be excluded from the list.
    """
    test_list = [x[0] for x in self._test_status.items()
                 if self._StatusOfTest(x[0]) == status]

    if not include_fails:
      test_list = [x for x in test_list if x.find('FAILS_') == -1]
    if not include_flaky:
      test_list = [x for x in test_list if x.find('FLAKY_') == -1]

    return test_list

  def _RecordError(self, line, reason):
    """Record a log line that produced a parsing error.

    Args:
      line: text of the line at which the error occurred
      reason: a string describing the error
    """
    self._internal_error_lines.append('%s: %s [%s]' %
                                      (self._line_number, line.strip(), reason))

  def RunningTests(self):
    """Returns list of tests that appear to be currently running."""
    return self._TestsByStatus('started', True, True)

  def ParsingErrors(self):
    """Returns a list of lines that have caused parsing errors."""
    return self._internal_error_lines

  def ClearParsingErrors(self):
    """Clears the currently stored parsing errors."""
    self._internal_error_lines = ['Cleared.']

  def PassedTests(self, include_fails=False, include_flaky=False):
    """Returns list of tests that passed."""
    return self._TestsByStatus('OK', include_fails, include_flaky)

  def FailedTests(self, include_fails=False, include_flaky=False):
    """Returns list of tests that failed, timed out, or didn't finish
    (crashed).

    This list will be incorrect until the complete log has been processed,
    because it will show currently running tests as having failed.

    Args:
      include_fails: If true, all failing tests with FAILS_ in their names will
          be included. Otherwise, they will only be included if they crashed or
          timed out.
      include_flaky: If true, all failing tests with FLAKY_ in their names will
          be included. Otherwise, they will only be included if they crashed or
          timed out.

    """
    return (self._TestsByStatus('failed', include_fails, include_flaky) +
            self._TestsByStatus('timeout', True, True) +
            self._TestsByStatus('warning', include_fails, include_flaky) +
            self.RunningTests())

  def TriesForTest(self, test):
    """Returns a list containing the state for all tries of the given test.
    This parser doesn't support retries so a single result is returned."""
    return [self.TEST_STATUS_MAP.get(self._StatusOfTest(test),
                                    TEST_UNKNOWN_LABEL)]

  def DisabledTests(self):
    """Returns the name of the disabled test (if there is only 1) or the number
    of disabled tests.
    """
    return self._disabled_tests

  def FlakyTests(self):
    """Returns the name of the flaky test (if there is only 1) or the number
    of flaky tests.
    """
    return self._flaky_tests

  def FailureDescription(self, test):
    """Returns a list containing the failure description for the given test.

    If the test didn't fail or timeout, returns [].
    """
    test_status = self._test_status.get(test, ('', []))
    return ['%s: ' % test] + test_status[1]

  def MemoryToolReportHashes(self):
    """Returns list of report hashes found in the log."""
    return self._memory_tool_reports.keys()

  def MemoryToolReport(self, report_hash):
    """Returns a list containing the report for a given hash.

    If the report hash doesn't exist, returns [].
    """
    return self._memory_tool_reports.get(report_hash, [])

  def CompletedWithoutFailure(self):
    """Returns True if all tests completed and no tests failed unexpectedly."""
    return self.completed and not self.FailedTests()

  def ProcessLine(self, line):
    """This is called once with each line of the test log."""

    # Track line number for error messages.
    self._line_number += 1

    # Some tests (net_unittests in particular) run subprocesses which can write
    # stuff to shared stdout buffer. Sometimes such output appears between new
    # line and gtest directives ('[  RUN  ]', etc) which breaks the parser.
    # Code below tries to detect such cases and recognize a mixed line as two
    # separate lines.

    # List of regexps that parses expects to find at the start of a line but
    # which can be somewhere in the middle.
    gtest_regexps = [
      self._test_start,
      self._test_ok,
      self._test_fail,
      self._test_passed,
    ]

    for regexp in gtest_regexps:
      match = regexp.search(line)
      if match:
        break

    if not match or match.start() == 0:
      self._ProcessLine(line)
    else:
      self._ProcessLine(line[:match.start()])
      self._ProcessLine(line[match.start():])

  def _ProcessLine(self, line):
    """Parses the line and changes the state of parsed tests accordingly.

    Will recognize newly started tests, OK or FAILED statuses, timeouts, etc.
    """

    # Note: When sharding, the number of disabled and flaky tests will be read
    # multiple times, so this will only show the most recent values (but they
    # should all be the same anyway).

    # Is it a line listing the master name?
    if not self.master_name:
      results = self._master_name_re.match(line)
      if results:
        self.master_name = results.group(1)

    results = self._run_test_cases_line.match(line)
    if results:
      # A run_test_cases.py output.
      if self._current_test:
        if self._test_status[self._current_test][0] == 'started':
          self._test_status[self._current_test] = (
              'timeout', self._failure_description)
      self._current_test = ''
      self._failure_description = []
      return

    # Is it a line declaring all tests passed?
    results = self._test_passed.match(line)
    if results:
      self.completed = True
      self._current_test = ''
      return

    # Is it a line reporting disabled tests?
    results = self._disabled.match(line)
    if results:
      try:
        disabled = int(results.group(1))
      except ValueError:
        disabled = 0
      if disabled > 0 and isinstance(self._disabled_tests, int):
        self._disabled_tests = disabled
      else:
        # If we can't parse the line, at least give a heads-up. This is a
        # safety net for a case that shouldn't happen but isn't a fatal error.
        self._disabled_tests = 'some'
      return

    # Is it a line reporting flaky tests?
    results = self._flaky.match(line)
    if results:
      try:
        flaky = int(results.group(1))
      except ValueError:
        flaky = 0
      if flaky > 0 and isinstance(self._flaky_tests, int):
        self._flaky_tests = flaky
      else:
        # If we can't parse the line, at least give a heads-up. This is a
        # safety net for a case that shouldn't happen but isn't a fatal error.
        self._flaky_tests = 'some'
      return

    # Is it the start of a test?
    results = self._test_start.match(line)
    if results:
      if self._current_test:
        if self._test_status[self._current_test][0] == 'started':
          self._test_status[self._current_test] = (
              'timeout', self._failure_description)
      test_name = results.group(1)
      self._test_status[test_name] = ('started', ['Did not complete.'])
      self._current_test = test_name
      if self.retrying_failed:
        self._failure_description = self._test_status[test_name][1]
        self._failure_description.extend(['', 'RETRY OUTPUT:', ''])
      else:
        self._failure_description = []
      return

    # Is it a test success line?
    results = self._test_ok.match(line)
    if results:
      test_name = results.group(1)
      status = self._StatusOfTest(test_name)
      if status != 'started':
        self._RecordError(line, 'success while in status %s' % status)
      if self.retrying_failed:
        self._test_status[test_name] = ('warning', self._failure_description)
      else:
        self._test_status[test_name] = ('OK', [])
      self._failure_description = []
      self._current_test = ''
      return

    # Is it a test failure line?
    results = self._test_fail.match(line)
    if results:
      test_name = results.group(1)
      status = self._StatusOfTest(test_name)
      if status not in ('started', 'failed', 'timeout'):
        self._RecordError(line, 'failure while in status %s' % status)
      # Don't overwrite the failure description when a failing test is listed a
      # second time in the summary, or if it was already recorded as timing
      # out.
      if status not in ('failed', 'timeout'):
        self._test_status[test_name] = ('failed', self._failure_description)
      self._failure_description = []
      self._current_test = ''
      return

    # Is it a test timeout line?
    results = self._test_timeout.search(line)
    if results:
      test_name = results.group(1)
      status = self._StatusOfTest(test_name)
      if status not in ('started', 'failed'):
        self._RecordError(line, 'timeout while in status %s' % status)
      self._test_status[test_name] = (
          'timeout', self._failure_description + ['Killed (timed out).'])
      self._failure_description = []
      self._current_test = ''
      return

    # Is it the start of a new memory tool report?
    results = self._report_start.match(line)
    if results:
      report_hash = results.group(1)
      if report_hash in self._memory_tool_reports:
        self._RecordError(line, 'multiple reports for this hash')
      self._memory_tool_reports[report_hash] = []
      self._current_report_hash = report_hash
      self._current_report = []
      return

    # Is it the end of a memory tool report?
    results = self._report_end.match(line)
    if results:
      report_hash = results.group(1)
      if not self._current_report_hash:
        self._RecordError(line, 'no BEGIN matches this END')
      elif report_hash != self._current_report_hash:
        self._RecordError(line, 'expected (error hash=#%s#)' %
            self._current_report_hash)
      else:
        self._memory_tool_reports[self._current_report_hash] = (
            self._current_report)
      self._current_report_hash = ''
      self._current_report = []
      return

    # Is it the start of the retry tests?
    results = self._retry_message.match(line)
    if results:
      self.retrying_failed = True
      return

    # Random line: if we're in a report, collect it. Reports are
    # generated after all tests are finished, so this should always belong to
    # the current report hash.
    if self._current_report_hash:
      self._current_report.append(line)
      return

    # Random line: if we're in a test, collect it for the failure description.
    # Tests may run simultaneously, so this might be off, but it's worth a try.
    # This also won't work if a test times out before it begins running.
    if self._current_test:
      self._failure_description.append(line)

    # Parse the "Failing tests:" list at the end of the output, and add any
    # additional failed tests to the list. For example, this includes tests
    # that crash after the OK line.
    if self._parsing_failures:
      results = self._test_name.match(line)
      if results:
        test_name = results.group(1)
        status = self._StatusOfTest(test_name)
        if status in ('not known', 'OK'):
          self._test_status[test_name] = (
              'failed', ['Unknown error, see stdio log.'])
      else:
        self._parsing_failures = False
    elif line.startswith('Failing tests:'):
      self._parsing_failures = True


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
