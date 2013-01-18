# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import json
import logging
import os
import re
import time
import traceback

from pylib import buildbot_report
from pylib import constants
from pylib.utils import flakiness_dashboard_results_uploader


_STAGING_SERVER = 'chrome-android-staging'


class BaseTestResult(object):
  """A single result from a unit test."""

  def __init__(self, name, log):
    self.name = name
    self.log = log.replace('\r', '')


class SingleTestResult(BaseTestResult):
  """Result information for a single test.

  Args:
    full_name: Full name of the test.
    start_date: Date in milliseconds when the test began running.
    dur: Duration of the test run in milliseconds.
    log: An optional string listing any errors.
  """

  def __init__(self, full_name, start_date, dur, log=''):
    BaseTestResult.__init__(self, full_name, log)
    name_pieces = full_name.rsplit('#')
    if len(name_pieces) > 1:
      self.test_name = name_pieces[1]
      self.class_name = name_pieces[0]
    else:
      self.class_name = full_name
      self.test_name = full_name
    self.start_date = start_date
    self.dur = dur


class TestResults(object):
  """Results of a test run."""

  def __init__(self):
    self.ok = []
    self.failed = []
    self.crashed = []
    self.unknown = []
    self.timed_out = []
    self.overall_timed_out = False
    self.overall_fail = False
    self.device_exception = None

  @staticmethod
  def FromRun(ok=None, failed=None, crashed=None, timed_out=None,
              overall_timed_out=False, overall_fail=False,
              device_exception=None):
    ret = TestResults()
    ret.ok = ok or []
    ret.failed = failed or []
    ret.crashed = crashed or []
    ret.timed_out = timed_out or []
    ret.overall_timed_out = overall_timed_out
    ret.overall_fail = overall_fail
    ret.device_exception = device_exception
    return ret

  @staticmethod
  def FromTestResults(results):
    """Combines a list of results in a single TestResults object."""
    ret = TestResults()
    for t in results:
      ret.ok += t.ok
      ret.failed += t.failed
      ret.crashed += t.crashed
      ret.unknown += t.unknown
      ret.timed_out += t.timed_out
      if t.overall_timed_out:
        ret.overall_timed_out = True
      if t.overall_fail:
        ret.overall_fail = True
    return ret

  @staticmethod
  def FromPythonException(test_name, start_date_ms, exc_info):
    """Constructs a TestResults with exception information for the given test.

    Args:
      test_name: name of the test which raised an exception.
      start_date_ms: the starting time for the test.
      exc_info: exception info, ostensibly from sys.exc_info().

    Returns:
      A TestResults object with a SingleTestResult in the failed list.
    """
    exc_type, exc_value, exc_traceback = exc_info
    trace_info = ''.join(traceback.format_exception(exc_type, exc_value,
                                                    exc_traceback))
    log_msg = 'Exception:\n' + trace_info
    duration_ms = (int(time.time()) * 1000) - start_date_ms

    exc_result = SingleTestResult(
                     full_name='PythonWrapper#' + test_name,
                     start_date=start_date_ms,
                     dur=duration_ms,
                     log=(str(exc_type) + ' ' + log_msg))

    results = TestResults()
    results.failed.append(exc_result)
    return results

  @staticmethod
  def DeviceExceptions(results):
    return set(filter(lambda t: t.device_exception, results))

  def _Log(self, sorted_list):
    for t in sorted_list:
      logging.critical(t.name)
      if t.log:
        logging.critical(t.log)

  def GetAllBroken(self):
    """Returns the all broken tests."""
    return self.failed + self.crashed + self.unknown + self.timed_out

  def _LogToFile(self, test_type, test_suite, build_type):
    """Log results to local files which can be used for aggregation later."""
    # TODO(frankf): Report tests that failed to run here too.
    log_file_path = os.path.join(constants.CHROME_DIR, 'out',
                                 build_type, 'test_logs')
    if not os.path.exists(log_file_path):
      os.mkdir(log_file_path)
    full_file_name = os.path.join(
        log_file_path, re.sub('\W', '_', test_type).lower() + '.log')
    if not os.path.exists(full_file_name):
      with open(full_file_name, 'w') as log_file:
        print >> log_file, '\n%s results for %s build %s:' % (
            test_type, os.environ.get('BUILDBOT_BUILDERNAME'),
            os.environ.get('BUILDBOT_BUILDNUMBER'))
      logging.info('Writing results to %s.' % full_file_name)
    log_contents = ['  %s result : %d tests ran' % (test_suite,
                                                    len(self.ok) +
                                                    len(self.failed) +
                                                    len(self.crashed) +
                                                    len(self.timed_out) +
                                                    len(self.unknown))]
    content_pairs = [('passed', len(self.ok)),
                     ('failed', len(self.failed)),
                     ('crashed', len(self.crashed)),
                     ('timed_out', len(self.timed_out)),
                     ('unknown', len(self.unknown))]
    for (result, count) in content_pairs:
      if count:
        log_contents.append(', %d tests %s' % (count, result))
    with open(full_file_name, 'a') as log_file:
      print >> log_file, ''.join(log_contents)
    logging.info('Writing results to %s.' % full_file_name)
    content = {'test_group': test_type,
               'ok': [t.name for t in self.ok],
               'failed': [t.name for t in self.failed],
               'crashed': [t.name for t in self.failed],
               'timed_out': [t.name for t in self.timed_out],
               'unknown': [t.name for t in self.unknown],}
    json_file_path = os.path.join(log_file_path, 'results.json')
    with open(json_file_path, 'a') as json_file:
      print >> json_file, json.dumps(content)
    logging.info('Writing results to %s.' % json_file_path)

  def _LogToFlakinessDashboard(self, test_type, test_package, flakiness_server):
    """Upload results to the flakiness dashboard"""
    logging.info('Upload results for test type "%s", test package "%s" to %s' %
                 (test_type, test_package, flakiness_server))

    # TODO(frankf): Enable uploading for gtests.
    if test_type != 'Instrumentation':
      logging.warning('Invalid test type.')
      return

    try:
      # TODO(frankf): Temp server for initial testing upstream.
      # Use http://test-results.appspot.com once we're confident this works.
      if _STAGING_SERVER in flakiness_server:
          assert test_package in ['ContentShellTest',
                                  'ChromiumTestShellTest',
                                  'AndroidWebViewTest']
          dashboard_test_type = ('%s_instrumentation_tests' %
                                 test_package.lower().rstrip('test'))
      # Downstream prod server.
      else:
        dashboard_test_type = 'Chromium_Android_Instrumentation'

      flakiness_dashboard_results_uploader.Upload(
          flakiness_server, dashboard_test_type, self)
    except Exception as e:
      logging.error(e)

  def LogFull(self, test_type, test_package, annotation=None,
              build_type='Debug', all_tests=None, flakiness_server=None):
    """Log the tests results for the test suite.

    The results will be logged three different ways:
      1. Log to stdout.
      2. Log to local files for aggregating multiple test steps
         (on buildbots only).
      3. Log to flakiness dashboard (on buildbots only).

    Args:
      test_type: Type of the test (e.g. 'Instrumentation', 'Unit test', etc.).
      test_package: Test package name (e.g. 'ipc_tests' for gtests,
                    'ContentShellTest' for instrumentation tests)
      annotation: If instrumenation test type, this is a list of annotations
                  (e.g. ['Smoke', 'SmallTest']).
      build_type: Release/Debug
      all_tests: A list of all tests that were supposed to run.
                 This is used to determine which tests have failed to run.
                 If None, we assume all tests ran.
      flakiness_server: If provider, upload the results to flakiness dashboard
                        with this URL.
      """
    # Output all broken tests or 'passed' if none broken.
    logging.critical('*' * 80)
    logging.critical('Final result:')
    if self.failed:
      logging.critical('Failed:')
      self._Log(sorted(self.failed))
    if self.crashed:
      logging.critical('Crashed:')
      self._Log(sorted(self.crashed))
    if self.timed_out:
      logging.critical('Timed out:')
      self._Log(sorted(self.timed_out))
    if self.unknown:
      logging.critical('Unknown:')
      self._Log(sorted(self.unknown))
    if not self.GetAllBroken():
      logging.critical('Passed')

    # Summarize in the test output.
    logging.critical('*' * 80)
    summary = ['Summary:\n']
    if all_tests:
      summary += ['TESTS_TO_RUN=%d\n' % len(all_tests)]
    num_tests_ran = (len(self.ok) + len(self.failed) +
                     len(self.crashed) + len(self.unknown) +
                     len(self.timed_out))
    tests_passed = [t.name for t in self.ok]
    tests_failed = [t.name for t in self.failed]
    tests_crashed = [t.name for t in self.crashed]
    tests_unknown = [t.name for t in self.unknown]
    tests_timed_out = [t.name for t in self.timed_out]
    summary += ['RAN=%d\n' % (num_tests_ran),
                'PASSED=%d\n' % len(tests_passed),
                'FAILED=%d %s\n' % (len(tests_failed), tests_failed),
                'CRASHED=%d %s\n' % (len(tests_crashed), tests_crashed),
                'TIMEDOUT=%d %s\n' % (len(tests_timed_out), tests_timed_out),
                'UNKNOWN=%d %s\n' % (len(tests_unknown), tests_unknown)]
    if all_tests and num_tests_ran != len(all_tests):
      # Add the list of tests we failed to run.
      tests_failed_to_run = list(set(all_tests) - set(tests_passed) -
                            set(tests_failed) - set(tests_crashed) -
                            set(tests_unknown) - set(tests_timed_out))
      summary += ['FAILED_TO_RUN=%d %s\n' % (len(tests_failed_to_run),
                                             tests_failed_to_run)]
    summary_string = ''.join(summary)
    logging.critical(summary_string)
    logging.critical('*' * 80)

    if os.environ.get('BUILDBOT_BUILDERNAME'):
      # It is possible to have multiple buildbot steps for the same
      # instrumenation test package using different annotations.
      if annotation and len(annotation) == 1:
        test_suite = annotation[0]
      else:
        test_suite = test_package
      self._LogToFile(test_type, test_suite, build_type)

      if flakiness_server:
        self._LogToFlakinessDashboard(test_type, test_package, flakiness_server)

  def PrintAnnotation(self):
    """Print buildbot annotations for test results."""
    if (self.failed or self.crashed or self.overall_fail or
        self.overall_timed_out):
      buildbot_report.PrintError()
    else:
      print 'Step success!'  # No annotation needed
