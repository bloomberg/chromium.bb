#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Basic pyauto performance tests.

For tests that need to be run for multiple iterations (e.g., so that average
and standard deviation values can be reported), the default number of iterations
run for each of these tests is specified by |_DEFAULT_NUM_ITERATIONS|.
That value can optionally be tweaked by setting an environment variable
'NUM_ITERATIONS' to a positive integer, representing the number of iterations
to run.
"""

import logging
import math
import os
import timeit

import pyauto_functional  # Must be imported before pyauto.
import pyauto


class PerfTest(pyauto.PyUITest):
  """Basic performance tests."""

  _DEFAULT_NUM_ITERATIONS = 50
  _PERF_OUTPUT_MARKER_PRE = '_PERF_PRE_'
  _PERF_OUTPUT_MARKER_POST = '_PERF_POST_'

  def setUp(self):
    """Performs necessary setup work before running each test."""
    self._num_iterations = self._DEFAULT_NUM_ITERATIONS
    if 'NUM_ITERATIONS' in os.environ:
      try:
        self._num_iterations = int(os.environ['NUM_ITERATIONS'])
        if self._num_iterations <= 0:
          raise ValueError('Environment variable NUM_ITERATIONS must be an '
                           'integer > 0.')
      except ValueError, e:
        self.fail('Error processing environment variable: %s' % e)
    pyauto.PyUITest.setUp(self)

  # TODO(dennisjeffrey): Reorganize the code to create a base PerfTest class
  # to separate out common functionality, then create specialized subclasses
  # such as TabPerfTest that implement the test-specific functionality.
  def _MeasureElapsedTime(self, python_command, num_invocations):
    """Measures time (in msec) to execute a python command one or more times.

    Args:
      python_command: A callable.
      num_invocations: An integer number of times to invoke the given command.

    Returns:
      The time required to execute the python command the specified number of
      times, in milliseconds as a float.
    """
    assert callable(python_command)
    def RunCommand():
      for _ in range(num_invocations):
        python_command()
    timer = timeit.Timer(stmt=lambda: RunCommand())
    return timer.timeit(number=1) * 1000  # Convert seconds to milliseconds.

  def _AvgAndStdDev(self, values):
    """Computes the average and standard deviation of a set of values.

    Args:
      values: A list of numeric values.

    Returns:
      A 2-tuple of floats (average, standard_deviation).
    """
    avg = 0.0
    std_dev = 0.0
    if values:
      avg = float(sum(values)) / len(values)
      if len(values) > 1:
        temp_vals = [math.pow(x - avg, 2) for x in values]
        std_dev = math.sqrt(sum(temp_vals) / (len(temp_vals) - 1))
    return avg, std_dev

  def _OutputPerfGraphValue(self, description, value):
    """Outputs a performance value to have it graphed on the performance bots.

    Only used for ChromeOS.

    Args:
      description: A string description of the performance value.
      value: A numeric value representing a single performance measurement.
    """
    if self.IsChromeOS():
      print '\n%s(\'%s\', %.2f)%s' % (self._PERF_OUTPUT_MARKER_PRE, description,
                                      value, self._PERF_OUTPUT_MARKER_POST)

  def _PrintSummaryResults(self, description, first_val, units, values=[]):
    """Logs summary measurement information.

    Args:
      description: A string description for the specified results.
      first_val: A numeric measurement value for a single initial trial.
      units: A string specifying the units for the specified measurements.
      values: A list of numeric value measurements.
    """
    logging.info('Results for: ' + description)
    logging.debug('Single trial: %.2f %s', first_val, units)
    if values:
      avg, std_dev = self._AvgAndStdDev(values)
      logging.info('Number of iterations: %d', len(values))
      for val in values:
        logging.info('  %.2f %s', val, units)
      logging.info('  --------------------------')
      logging.info('  Average: %.2f %s', avg, units)
      logging.info('  Std dev: %.2f %s', std_dev, units)
      self._OutputPerfGraphValue('%s_%s' % (units, description), avg)
    else:
      self._OutputPerfGraphValue('%s_%s' % (units, description), first_val)

  def _RunNewTabTest(self, description, open_tab_command, num_tabs=1):
    """Runs a perf test that involves opening new tab(s).

    This helper function can be called from different tests to do perf testing
    with different types of tabs.  It is assumed that the |open_tab_command|
    will open up a single tab.

    Args:
      description: A string description of the associated tab test.
      open_tab_command: A callable that will open a single tab.
      num_tabs: The number of tabs to open, i.e., the number of times to invoke
                the |open_tab_command|.
    """
    assert callable(open_tab_command)

    # TODO(dennisjeffrey): Consider not taking an initial sample here.
    orig_elapsed = self._MeasureElapsedTime(open_tab_command, num_tabs)
    self.assertEqual(1 + num_tabs, self.GetTabCount(),
                     msg='Did not open %d new tab(s).' % num_tabs)
    for _ in range(num_tabs):
      self.GetBrowserWindow(0).GetTab(1).Close(True)

    timings = []
    for _ in range(self._num_iterations):
      elapsed = self._MeasureElapsedTime(open_tab_command, num_tabs)
      self.assertEqual(1 + num_tabs, self.GetTabCount(),
                       msg='Did not open %d new tab(s).' % num_tabs)
      for _ in range(num_tabs):
        self.GetBrowserWindow(0).GetTab(1).Close(True)
      timings.append(elapsed)

    self._PrintSummaryResults(description, orig_elapsed, 'milliseconds',
                              values=timings)

  def testNewTab(self):
    """Measures time to open a new tab."""
    self._RunNewTabTest('NewTabPage',
                        lambda: self.AppendTab(pyauto.GURL('chrome://newtab')))

  def testNewTabPdf(self):
    """Measures time to open a new tab navigated to a PDF file."""
    url = self.GetFileURLForDataPath('pyauto_private', 'pdf', 'TechCrunch.pdf')
    self._RunNewTabTest('NewTabPdfPage',
                        lambda: self.AppendTab(pyauto.GURL(url)))

  def testNewTabFlash(self):
    """Measures time to open a new tab navigated to a flash page."""
    url = self.GetFileURLForDataPath('plugin', 'flash.swf')
    self._RunNewTabTest('NewTabFlashPage',
                        lambda: self.AppendTab(pyauto.GURL(url)))

  def test20Tabs(self):
    """Measures time to open 20 tabs."""
    self._RunNewTabTest(
        '20TabsNewTabPage',
        lambda: self.AppendTab(pyauto.GURL('chrome://newtab')), num_tabs=20)

  def testV8BenchmarkSuite(self):
    """Measures score from online v8 benchmark suite."""
    url = self.GetFileURLForDataPath('v8_benchmark_v6', 'run.html')
    self.AppendTab(pyauto.GURL(url))
    js = """
        var val = document.getElementById("status").innerHTML;
        window.domAutomationController.send(val);
    """
    self.assertTrue(
        self.WaitUntil(
            lambda: 'Score:' in self.ExecuteJavascript(js, 0, 1), timeout=300,
            expect_retval=True),
        msg='Timed out when waiting for v8 benchmark score.')
    val = self.ExecuteJavascript(js, 0, 1)
    score = int(val[val.find(':') + 2:])
    self._PrintSummaryResults('V8Benchmark', score, 'score')


if __name__ == '__main__':
  pyauto_functional.Main()
