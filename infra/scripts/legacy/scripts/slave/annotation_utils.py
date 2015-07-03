# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates annotated output.

TODO(stip): Move the gtest_utils gtest parser selection code from runtest.py
to here.
TODO(stip): Move the perf dashboard code from runtest.py to here.
"""

import re

from slave import performance_log_processor
from slave import slave_utils


def getText(result, observer, name):
  """Generate a text summary for the waterfall.

  Updates the waterfall with any unusual test output, with a link to logs of
  failed test steps.
  """
  GTEST_DASHBOARD_BASE = ('http://test-results.appspot.com'
                          '/dashboards/flakiness_dashboard.html')

  # TODO(xusydoc): unify this with gtest reporting below so getText() is
  # less confusing
  if hasattr(observer, 'PerformanceSummary'):
    basic_info = [name]
    summary_text = ['<div class="BuildResultInfo">']
    summary_text.extend(observer.PerformanceSummary())
    summary_text.append('</div>')
    return basic_info + summary_text

  # basic_info is an array of lines to display on the waterfall.
  basic_info = [name]

  disabled = observer.DisabledTests()
  if disabled:
    basic_info.append('%s disabled' % str(disabled))

  flaky = observer.FlakyTests()
  if flaky:
    basic_info.append('%s flaky' % str(flaky))

  failed_test_count = len(observer.FailedTests())
  if failed_test_count == 0:
    if result == performance_log_processor.SUCCESS:
      return basic_info
    elif result == performance_log_processor.WARNINGS:
      return basic_info + ['warnings']

  if observer.RunningTests():
    basic_info += ['did not complete']

  # TODO(xusydoc): see if 'crashed or hung' should be tracked by RunningTests().
  if failed_test_count:
    failure_text = ['failed %d' % failed_test_count]
    if observer.master_name:
      # Include the link to the flakiness dashboard.
      failure_text.append('<div class="BuildResultInfo">')
      failure_text.append('<a href="%s#testType=%s'
                          '&tests=%s">' % (GTEST_DASHBOARD_BASE,
                                           name,
                                           ','.join(observer.FailedTests())))
      failure_text.append('Flakiness dashboard')
      failure_text.append('</a>')
      failure_text.append('</div>')
  else:
    failure_text = ['crashed or hung']
  return basic_info + failure_text


def annotate(test_name, result, log_processor, perf_dashboard_id=None):
  """Given a test result and tracker, update the waterfall with test results."""

  # Always print raw exit code of the subprocess. This is very helpful
  # for debugging, especially when one gets the "crashed or hung" message
  # with no output (exit code can have some clues, especially on Windows).
  print 'exit code (as seen by runtest.py): %d' % result

  get_text_result = performance_log_processor.SUCCESS

  for failure in sorted(log_processor.FailedTests()):
    clean_test_name = re.sub(r'[^\w\.\-]', '_', failure)
    slave_utils.WriteLogLines(clean_test_name,
                              log_processor.FailureDescription(failure))
  for report_hash in sorted(log_processor.MemoryToolReportHashes()):
    slave_utils.WriteLogLines(report_hash,
                              log_processor.MemoryToolReport(report_hash))

  if log_processor.ParsingErrors():
    # Generate a log file containing the list of errors.
    slave_utils.WriteLogLines('log parsing error(s)',
                              log_processor.ParsingErrors())

    log_processor.ClearParsingErrors()

  if hasattr(log_processor, 'evaluateCommand'):
    parser_result = log_processor.evaluateCommand('command')
    if parser_result > result:
      result = parser_result

  if result == performance_log_processor.SUCCESS:
    if (len(log_processor.ParsingErrors()) or
        len(log_processor.FailedTests()) or
        len(log_processor.MemoryToolReportHashes())):
      print '@@@STEP_WARNINGS@@@'
      get_text_result = performance_log_processor.WARNINGS
  elif result == slave_utils.WARNING_EXIT_CODE:
    print '@@@STEP_WARNINGS@@@'
    get_text_result = performance_log_processor.WARNINGS
  else:
    print '@@@STEP_FAILURE@@@'
    get_text_result = performance_log_processor.FAILURE

  for desc in getText(get_text_result, log_processor, test_name):
    print '@@@STEP_TEXT@%s@@@' % desc

  if hasattr(log_processor, 'PerformanceLogs'):
    if not perf_dashboard_id:
      raise Exception('runtest.py error: perf step specified but'
                      'no test_id in factory_properties!')
    for logname, log in log_processor.PerformanceLogs().iteritems():
      lines = [str(l).rstrip() for l in log]
      slave_utils.WriteLogLines(logname, lines, perf=perf_dashboard_id)
