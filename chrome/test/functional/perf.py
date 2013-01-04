#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Basic pyauto performance tests.

For tests that need to be run for multiple iterations (e.g., so that average
and standard deviation values can be reported), the default number of iterations
run for each of these tests is specified by |_DEFAULT_NUM_ITERATIONS|.
That value can optionally be tweaked by setting an environment variable
'NUM_ITERATIONS' to a positive integer, representing the number of iterations
to run.  An additional, initial iteration will also be run to "warm up" the
environment, and the result from that initial iteration will be ignored.

Some tests rely on repeatedly appending tabs to Chrome.  Occasionally, these
automation calls time out, thereby affecting the timing measurements (see issue
crosbug.com/20503).  To work around this, the tests discard timing measurements
that involve automation timeouts.  The value |_DEFAULT_MAX_TIMEOUT_COUNT|
specifies the threshold number of timeouts that can be tolerated before the test
fails.  To tweak this value, set environment variable 'MAX_TIMEOUT_COUNT' to the
desired threshold value.
"""

import BaseHTTPServer
import commands
import errno
import itertools
import logging
import math
import os
import posixpath
import re
import SimpleHTTPServer
import SocketServer
import signal
import subprocess
import sys
import tempfile
import threading
import time
import timeit
import urllib
import urllib2
import urlparse

import pyauto_functional  # Must be imported before pyauto.
import pyauto
import simplejson  # Must be imported after pyauto; located in third_party.

from netflix import NetflixTestHelper
import pyauto_utils
import test_utils
import webpagereplay
from youtube import YoutubeTestHelper


_CHROME_BASE_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, os.pardir, os.pardir))


def FormatChromePath(posix_path, **kwargs):
  """Convert a path relative to the Chromium root into an OS-specific path.

  Args:
    posix_path: a path string that may be a format().
      Example: 'src/third_party/{module_name}/__init__.py'
    kwargs: args for the format replacement.
      Example: {'module_name': 'pylib'}

  Returns:
    an absolute path in the current Chromium tree with formatting applied.
  """
  formated_path = posix_path.format(**kwargs)
  path_parts = formated_path.split('/')
  return os.path.join(_CHROME_BASE_DIR, *path_parts)


def StandardDeviation(values):
  """Returns the standard deviation of |values|."""
  avg = Mean(values)
  if len(values) < 2 or not avg:
    return 0.0
  temp_vals = [math.pow(x - avg, 2) for x in values]
  return math.sqrt(sum(temp_vals) / (len(temp_vals) - 1))


def Mean(values):
  """Returns the arithmetic mean of |values|."""
  if not values or None in values:
    return None
  return sum(values) / float(len(values))


def GeometricMean(values):
  """Returns the geometric mean of |values|."""
  if not values or None in values or [x for x in values if x < 0.0]:
    return None
  if 0.0 in values:
    return 0.0
  return math.exp(Mean([math.log(x) for x in values]))


class BasePerfTest(pyauto.PyUITest):
  """Base class for performance tests."""

  _DEFAULT_NUM_ITERATIONS = 10  # Keep synced with desktopui_PyAutoPerfTests.py.
  _DEFAULT_MAX_TIMEOUT_COUNT = 10
  _PERF_OUTPUT_MARKER_PRE = '_PERF_PRE_'
  _PERF_OUTPUT_MARKER_POST = '_PERF_POST_'

  def setUp(self):
    """Performs necessary setup work before running each test."""
    self._num_iterations = self._DEFAULT_NUM_ITERATIONS
    if 'NUM_ITERATIONS' in os.environ:
      self._num_iterations = int(os.environ['NUM_ITERATIONS'])
    self._max_timeout_count = self._DEFAULT_MAX_TIMEOUT_COUNT
    if 'MAX_TIMEOUT_COUNT' in os.environ:
      self._max_timeout_count = int(os.environ['MAX_TIMEOUT_COUNT'])
    self._timeout_count = 0

    # For users who want to see local perf graphs for Chrome when running the
    # tests on their own machines.
    self._local_perf_dir = None
    if 'LOCAL_PERF_DIR' in os.environ:
      self._local_perf_dir = os.environ['LOCAL_PERF_DIR']
      if not os.path.exists(self._local_perf_dir):
        self.fail('LOCAL_PERF_DIR environment variable specified as %s, '
                  'but this directory does not exist.' % self._local_perf_dir)
    # When outputting perf graph information on-the-fly for Chrome, this
    # variable lets us know whether a perf measurement is for a new test
    # execution, or the current test execution.
    self._seen_graph_lines = {}

    pyauto.PyUITest.setUp(self)

    # Flush all buffers to disk and wait until system calms down.  Must be done
    # *after* calling pyauto.PyUITest.setUp, since that is where Chrome is
    # killed and re-initialized for a new test.
    # TODO(dennisjeffrey): Implement wait for idle CPU on Windows/Mac.
    if self.IsLinux():  # IsLinux() also implies IsChromeOS().
      os.system('sync')
      self._WaitForIdleCPU(60.0, 0.05)

  def _IsPIDRunning(self, pid):
    """Checks if a given process id is running.

    Args:
      pid: The process id of the process to check.

    Returns:
      True if the process is running. False if not.
    """
    try:
      # Note that this sends the signal 0, which should not interfere with the
      # process.
      os.kill(pid, 0)
    except OSError, err:
      if err.errno == errno.ESRCH:
        return False
    return True

  def _WaitForChromeExit(self, browser_info, timeout):
    child_processes = browser_info['child_processes']
    initial_time = time.time()
    while time.time() - initial_time < timeout:
      if any([self._IsPIDRunning(c['pid']) for c in child_processes]):
        time.sleep(1)
      else:
        logging.info('_WaitForChromeExit() took: %s seconds',
                     time.time() - initial_time)
        return
    self.fail('_WaitForChromeExit() did not finish within %s seconds',
              timeout)

  def tearDown(self):
    if self._IsPGOMode():
      browser_info = self.GetBrowserInfo()
      pid = browser_info['browser_pid']
      os.kill(pid, signal.SIGINT)
      self._WaitForChromeExit(browser_info, 30)

    pyauto.PyUITest.tearDown(self)

  def _IsPGOMode(self):
    return 'USE_PGO' in os.environ

  def _WaitForIdleCPU(self, timeout, utilization):
    """Waits for the CPU to become idle (< utilization).

    Args:
      timeout: The longest time in seconds to wait before throwing an error.
      utilization: The CPU usage below which the system should be considered
          idle (between 0 and 1.0 independent of cores/hyperthreads).
    """
    time_passed = 0.0
    fraction_non_idle_time = 1.0
    logging.info('Starting to wait up to %fs for idle CPU...', timeout)
    while fraction_non_idle_time >= utilization:
      cpu_usage_start = self._GetCPUUsage()
      time.sleep(2)
      time_passed += 2.0
      cpu_usage_end = self._GetCPUUsage()
      fraction_non_idle_time = \
          self._GetFractionNonIdleCPUTime(cpu_usage_start, cpu_usage_end)
      logging.info('Current CPU utilization = %f.', fraction_non_idle_time)
      if time_passed > timeout:
        self._LogProcessActivity()
        message = ('CPU did not idle after %fs wait (utilization = %f).' % (
                   time_passed, fraction_non_idle_time))

        # crosbug.com/37389
        if self._IsPGOMode():
          logging.info(message)
          logging.info('Still continuing because we are in PGO mode.')
          return

        self.fail(message)
    logging.info('Wait for idle CPU took %fs (utilization = %f).',
                 time_passed, fraction_non_idle_time)

  def _LogProcessActivity(self):
    """Logs the output of top on Linux/Mac/CrOS.

       TODO: use taskmgr or similar on Windows.
    """
    if self.IsLinux() or self.IsMac():  # IsLinux() also implies IsChromeOS().
      logging.info('Logging current process activity using top.')
      cmd = 'top -b -d1 -n1'
      if self.IsMac():
        cmd = 'top -l1'
      p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE,
          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, close_fds=True)
      output = p.stdout.read()
      logging.info(output)
    else:
      logging.info('Process activity logging not implemented on this OS.')

  def _AppendTab(self, url):
    """Appends a tab and increments a counter if the automation call times out.

    Args:
      url: The string url to which the appended tab should be navigated.
    """
    if not self.AppendTab(pyauto.GURL(url)):
      self._timeout_count += 1

  def _MeasureElapsedTime(self, python_command, num_invocations=1):
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
    timer = timeit.Timer(stmt=RunCommand)
    return timer.timeit(number=1) * 1000  # Convert seconds to milliseconds.

  def _OutputPerfForStandaloneGraphing(self, graph_name, description, value,
                                       units, units_x, is_stacked):
    """Outputs perf measurement data to a local folder to be graphed.

    This function only applies to Chrome desktop, and assumes that environment
    variable 'LOCAL_PERF_DIR' has been specified and refers to a valid directory
    on the local machine.

    Args:
      graph_name: A string name for the graph associated with this performance
          value.
      description: A string description of the performance value.  Should not
          include spaces.
      value: Either a single numeric value representing a performance
          measurement, or else a list of (x, y) tuples representing one or more
          long-running performance measurements, where 'x' is an x-axis value
          (such as an iteration number) and 'y' is the corresponding performance
          measurement.  If a list of tuples is given, then the |units_x|
          argument must also be specified.
      units: A string representing the units of the performance measurement(s).
          Should not include spaces.
      units_x: A string representing the units of the x-axis values associated
          with the performance measurements, such as 'iteration' if the x values
          are iteration numbers.  If this argument is specified, then the
          |value| argument must be a list of (x, y) tuples.
      is_stacked: True to draw a "stacked" graph.  First-come values are
          stacked at bottom by default.
    """
    revision_num_file = os.path.join(self._local_perf_dir, 'last_revision.dat')
    if os.path.exists(revision_num_file):
      with open(revision_num_file) as f:
        revision = int(f.read())
    else:
      revision = 0

    if not self._seen_graph_lines:
      # We're about to output data for a new test run.
      revision += 1

    # Update graphs.dat.
    existing_graphs = []
    graphs_file = os.path.join(self._local_perf_dir, 'graphs.dat')
    if os.path.exists(graphs_file):
      with open(graphs_file) as f:
        existing_graphs = simplejson.loads(f.read())
    is_new_graph = True
    for graph in existing_graphs:
      if graph['name'] == graph_name:
        is_new_graph = False
        break
    if is_new_graph:
      new_graph =  {
        'name': graph_name,
        'units': units,
        'important': False,
      }
      if units_x:
        new_graph['units_x'] = units_x
      existing_graphs.append(new_graph)
      with open(graphs_file, 'w') as f:
        f.write(simplejson.dumps(existing_graphs))
      os.chmod(graphs_file, 0755)

    # Update data file for this particular graph.
    existing_lines = []
    data_file = os.path.join(self._local_perf_dir, graph_name + '-summary.dat')
    if os.path.exists(data_file):
      with open(data_file) as f:
        existing_lines = f.readlines()
    existing_lines = map(
        simplejson.loads, map(lambda x: x.strip(), existing_lines))

    seen_key = graph_name
    # We assume that the first line |existing_lines[0]| is the latest.
    if units_x:
      new_line = {
        'rev': revision,
        'traces': { description: [] }
      }
      if seen_key in self._seen_graph_lines:
        # We've added points previously for this graph line in the current
        # test execution, so retrieve the original set of points specified in
        # the most recent revision in the data file.
        new_line = existing_lines[0]
        if not description in new_line['traces']:
          new_line['traces'][description] = []
      for x_value, y_value in value:
        new_line['traces'][description].append([str(x_value), str(y_value)])
    else:
      new_line = {
        'rev': revision,
        'traces': { description: [str(value), str(0.0)] }
      }

    if is_stacked:
      new_line['stack'] = True
      if 'stack_order' not in new_line:
        new_line['stack_order'] = []
      if description not in new_line['stack_order']:
        new_line['stack_order'].append(description)

    if seen_key in self._seen_graph_lines:
      # Update results for the most recent revision.
      existing_lines[0] = new_line
    else:
      # New results for a new revision.
      existing_lines.insert(0, new_line)
      self._seen_graph_lines[seen_key] = True

    existing_lines = map(simplejson.dumps, existing_lines)
    with open(data_file, 'w') as f:
      f.write('\n'.join(existing_lines))
    os.chmod(data_file, 0755)

    with open(revision_num_file, 'w') as f:
      f.write(str(revision))

  def _OutputPerfGraphValue(self, description, value, units,
                            graph_name, units_x=None, is_stacked=False):
    """Outputs a performance value to have it graphed on the performance bots.

    The output format differs, depending on whether the current platform is
    Chrome desktop or ChromeOS.

    For ChromeOS, the performance bots have a 30-character limit on the length
    of the key associated with a performance value.  A key on ChromeOS is
    considered to be of the form "units_description" (for example,
    "milliseconds_NewTabPage"), and is created from the |units| and
    |description| passed as input to this function.  Any characters beyond the
    length 30 limit are truncated before results are stored in the autotest
    database.

    Args:
      description: A string description of the performance value.  Should not
          include spaces.
      value: Either a numeric value representing a performance measurement, or
          a list of values to be averaged. Lists may also contain (x, y) tuples
          representing one or more performance measurements, where 'x' is an
          x-axis value (such as an iteration number) and 'y' is the
          corresponding performance measurement.  If a list of tuples is given,
          the |units_x| argument must also be specified.
      units: A string representing the units of the performance measurement(s).
          Should not include spaces.
      graph_name: A string name for the graph associated with this performance
          value.  Only used on Chrome desktop.
      units_x: A string representing the units of the x-axis values associated
          with the performance measurements, such as 'iteration' if the x values
          are iteration numbers.  If this argument is specified, then the
          |value| argument must be a list of (x, y) tuples.
      is_stacked: True to draw a "stacked" graph.  First-come values are
          stacked at bottom by default.
    """
    if (isinstance(value, list) and value[0] is not None and
        isinstance(value[0], tuple)):
      assert units_x
    if units_x:
      assert isinstance(value, list)

    if self.IsChromeOS():
      # Autotest doesn't support result lists.
      autotest_value = value
      if (isinstance(value, list) and value[0] is not None and
          not isinstance(value[0], tuple)):
        autotest_value = Mean(value)

      if units_x:
        # TODO(dennisjeffrey): Support long-running performance measurements on
        # ChromeOS in a way that can be graphed: crosbug.com/21881.
        pyauto_utils.PrintPerfResult(graph_name, description, autotest_value,
                                     units + ' ' + units_x)
      else:
        # Output short-running performance results in a format understood by
        # autotest.
        perf_key = '%s_%s' % (units, description)
        if len(perf_key) > 30:
          logging.warning('The description "%s" will be truncated to "%s" '
                          '(length 30) when added to the autotest database.',
                          perf_key, perf_key[:30])
        print '\n%s(\'%s\', %f)%s' % (self._PERF_OUTPUT_MARKER_PRE,
                                        perf_key, autotest_value,
                                        self._PERF_OUTPUT_MARKER_POST)

        # Also output results in the format recognized by buildbot, for cases
        # in which these tests are run on chromeOS through buildbot.  Since
        # buildbot supports result lists, it's ok for |value| to be a list here.
        pyauto_utils.PrintPerfResult(graph_name, description, value, units)

        sys.stdout.flush()
    else:
      # TODO(dmikurube): Support stacked graphs in PrintPerfResult.
      # See http://crbug.com/122119.
      if units_x:
        pyauto_utils.PrintPerfResult(graph_name, description, value,
                                     units + ' ' + units_x)
      else:
        pyauto_utils.PrintPerfResult(graph_name, description, value, units)

      if self._local_perf_dir:
        self._OutputPerfForStandaloneGraphing(
            graph_name, description, value, units, units_x, is_stacked)

  def _OutputEventForStandaloneGraphing(self, description, event_list):
    """Outputs event information to a local folder to be graphed.

    See function _OutputEventGraphValue below for a description of an event.

    This function only applies to Chrome Endure tests running on Chrome desktop,
    and assumes that environment variable 'LOCAL_PERF_DIR' has been specified
    and refers to a valid directory on the local machine.

    Args:
      description: A string description of the event.  Should not include
          spaces.
      event_list: A list of (x, y) tuples representing one or more events
          occurring during an endurance test, where 'x' is the time of the event
          (in seconds since the start of the test), and 'y' is a dictionary
          representing relevant data associated with that event (as key/value
          pairs).
    """
    revision_num_file = os.path.join(self._local_perf_dir, 'last_revision.dat')
    if os.path.exists(revision_num_file):
      with open(revision_num_file) as f:
        revision = int(f.read())
    else:
      revision = 0

    if not self._seen_graph_lines:
      # We're about to output data for a new test run.
      revision += 1

    existing_lines = []
    data_file = os.path.join(self._local_perf_dir, '_EVENT_-summary.dat')
    if os.path.exists(data_file):
      with open(data_file) as f:
        existing_lines = f.readlines()
    existing_lines = map(eval, map(lambda x: x.strip(), existing_lines))

    seen_event_type = description
    value_list = []
    if seen_event_type in self._seen_graph_lines:
      # We've added events previously for this event type in the current
      # test execution, so retrieve the original set of values specified in
      # the most recent revision in the data file.
      value_list = existing_lines[0]['events'][description]
    for event_time, event_data in event_list:
      value_list.append([str(event_time), event_data])
    new_events = {
      description: value_list
    }

    new_line = {
      'rev': revision,
      'events': new_events
    }

    if seen_event_type in self._seen_graph_lines:
      # Update results for the most recent revision.
      existing_lines[0] = new_line
    else:
      # New results for a new revision.
      existing_lines.insert(0, new_line)
      self._seen_graph_lines[seen_event_type] = True

    existing_lines = map(str, existing_lines)
    with open(data_file, 'w') as f:
      f.write('\n'.join(existing_lines))
    os.chmod(data_file, 0755)

    with open(revision_num_file, 'w') as f:
      f.write(str(revision))

  def _OutputEventGraphValue(self, description, event_list):
    """Outputs a set of events to have them graphed on the Chrome Endure bots.

    An "event" can be anything recorded by a performance test that occurs at
    particular times during a test execution.  For example, a garbage collection
    in the v8 heap can be considered an event.  An event is distinguished from a
    regular perf measurement in two ways: (1) an event is depicted differently
    in the performance graphs than performance measurements; (2) an event can
    be associated with zero or more data fields describing relevant information
    associated with the event.  For example, a garbage collection event will
    occur at a particular time, and it may be associated with data such as
    the number of collected bytes and/or the length of time it took to perform
    the garbage collection.

    This function only applies to Chrome Endure tests running on Chrome desktop.

    Args:
      description: A string description of the event.  Should not include
          spaces.
      event_list: A list of (x, y) tuples representing one or more events
          occurring during an endurance test, where 'x' is the time of the event
          (in seconds since the start of the test), and 'y' is a dictionary
          representing relevant data associated with that event (as key/value
          pairs).
    """
    pyauto_utils.PrintPerfResult('_EVENT_', description, event_list, '')
    if self._local_perf_dir:
      self._OutputEventForStandaloneGraphing(description, event_list)

  def _PrintSummaryResults(self, description, values, units, graph_name):
    """Logs summary measurement information.

    This function computes and outputs the average and standard deviation of
    the specified list of value measurements.  It also invokes
    _OutputPerfGraphValue() with the computed *average* value, to ensure the
    average value can be plotted in a performance graph.

    Args:
      description: A string description for the specified results.
      values: A list of numeric value measurements.
      units: A string specifying the units for the specified measurements.
      graph_name: A string name for the graph associated with this performance
          value.  Only used on Chrome desktop.
    """
    logging.info('Overall results for: %s', description)
    if values:
      logging.info('  Average: %f %s', Mean(values), units)
      logging.info('  Std dev: %f %s', StandardDeviation(values), units)
      self._OutputPerfGraphValue(description, values, units, graph_name)
    else:
      logging.info('No results to report.')

  def _RunNewTabTest(self, description, open_tab_command, graph_name,
                     num_tabs=1):
    """Runs a perf test that involves opening new tab(s).

    This helper function can be called from different tests to do perf testing
    with different types of tabs.  It is assumed that the |open_tab_command|
    will open up a single tab.

    Args:
      description: A string description of the associated tab test.
      open_tab_command: A callable that will open a single tab.
      graph_name: A string name for the performance graph associated with this
          test.  Only used on Chrome desktop.
      num_tabs: The number of tabs to open, i.e., the number of times to invoke
          the |open_tab_command|.
    """
    assert callable(open_tab_command)

    timings = []
    for iteration in range(self._num_iterations + 1):
      orig_timeout_count = self._timeout_count
      elapsed_time = self._MeasureElapsedTime(open_tab_command,
                                              num_invocations=num_tabs)
      # Only count the timing measurement if no automation call timed out.
      if self._timeout_count == orig_timeout_count:
        # Ignore the first iteration.
        if iteration:
          timings.append(elapsed_time)
          logging.info('Iteration %d of %d: %f milliseconds', iteration,
                       self._num_iterations, elapsed_time)
      self.assertTrue(self._timeout_count <= self._max_timeout_count,
                      msg='Test exceeded automation timeout threshold.')
      self.assertEqual(1 + num_tabs, self.GetTabCount(),
                       msg='Did not open %d new tab(s).' % num_tabs)
      for _ in range(num_tabs):
        self.CloseTab(tab_index=1)

    self._PrintSummaryResults(description, timings, 'milliseconds', graph_name)

  def _GetConfig(self):
    """Load perf test configuration file.

    Returns:
      A dictionary that represents the config information.
    """
    config_file = os.path.join(os.path.dirname(__file__), 'perf.cfg')
    config = {'username': None,
              'password': None,
              'google_account_url': 'https://accounts.google.com/',
              'gmail_url': 'https://www.gmail.com',
              'plus_url': 'https://plus.google.com',
              'docs_url': 'https://docs.google.com'}
    if os.path.exists(config_file):
      try:
        new_config = pyauto.PyUITest.EvalDataFrom(config_file)
        for key in new_config:
          if new_config.get(key) is not None:
            config[key] = new_config.get(key)
      except SyntaxError, e:
        logging.info('Could not read %s: %s', config_file, str(e))
    return config

  def _LoginToGoogleAccount(self, account_key='test_google_account'):
    """Logs in to a test Google account.

    Login with user-defined credentials if they exist.
    Else login with private test credentials if they exist.
    Else fail.

    Args:
      account_key: The string key in private_tests_info.txt which is associated
                   with the test account login credentials to use. It will only
                   be used when fail to load user-defined credentials.

    Raises:
      RuntimeError: if could not get credential information.
    """
    private_file = os.path.join(pyauto.PyUITest.DataDir(), 'pyauto_private',
                                'private_tests_info.txt')
    config_file = os.path.join(os.path.dirname(__file__), 'perf.cfg')
    config = self._GetConfig()
    google_account_url = config.get('google_account_url')
    username = config.get('username')
    password = config.get('password')
    if username and password:
      logging.info(
          'Using google account credential from %s',
          os.path.join(os.path.dirname(__file__), 'perf.cfg'))
    elif os.path.exists(private_file):
      creds = self.GetPrivateInfo()[account_key]
      username = creds['username']
      password = creds['password']
      logging.info(
          'User-defined credentials not found,' +
          ' using private test credentials instead.')
    else:
      message = 'No user-defined or private test ' \
                'credentials could be found. ' \
                'Please specify credential information in %s.' \
                % config_file
      raise RuntimeError(message)
    test_utils.GoogleAccountsLogin(
        self, username, password, url=google_account_url)
    self.NavigateToURL('about:blank')  # Clear the existing tab.

  def _GetCPUUsage(self):
    """Returns machine's CPU usage.

    This function uses /proc/stat to identify CPU usage, and therefore works
    only on Linux/ChromeOS.

    Returns:
      A dictionary with 'user', 'nice', 'system' and 'idle' values.
      Sample dictionary:
      {
        'user': 254544,
        'nice': 9,
        'system': 254768,
        'idle': 2859878,
      }
    """
    try:
      f = open('/proc/stat')
      cpu_usage_str = f.readline().split()
      f.close()
    except IOError, e:
      self.fail('Could not retrieve CPU usage: ' + str(e))
    return {
      'user': int(cpu_usage_str[1]),
      'nice': int(cpu_usage_str[2]),
      'system': int(cpu_usage_str[3]),
      'idle': int(cpu_usage_str[4])
    }

  def _GetFractionNonIdleCPUTime(self, cpu_usage_start, cpu_usage_end):
    """Computes the fraction of CPU time spent non-idling.

    This function should be invoked using before/after values from calls to
    _GetCPUUsage().
    """
    time_non_idling_end = (cpu_usage_end['user'] + cpu_usage_end['nice'] +
                           cpu_usage_end['system'])
    time_non_idling_start = (cpu_usage_start['user'] + cpu_usage_start['nice'] +
                             cpu_usage_start['system'])
    total_time_end = (cpu_usage_end['user'] + cpu_usage_end['nice'] +
                      cpu_usage_end['system'] + cpu_usage_end['idle'])
    total_time_start = (cpu_usage_start['user'] + cpu_usage_start['nice'] +
                        cpu_usage_start['system'] + cpu_usage_start['idle'])
    return ((float(time_non_idling_end) - time_non_idling_start) /
            (total_time_end - total_time_start))

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    flags = super(BasePerfTest, self).ExtraChromeFlags()
    # Window size impacts a variety of perf tests, ensure consistency.
    flags.append('--window-size=1024,768')
    if self._IsPGOMode():
      flags = flags + ['--child-clean-exit', '--no-sandbox']
    return flags


class TabPerfTest(BasePerfTest):
  """Tests that involve opening tabs."""

  def testNewTab(self):
    """Measures time to open a new tab."""
    self._RunNewTabTest('NewTabPage',
                        lambda: self._AppendTab('chrome://newtab'), 'open_tab')

  def testNewTabFlash(self):
    """Measures time to open a new tab navigated to a flash page."""
    self.assertTrue(
        os.path.exists(os.path.join(self.ContentDataDir(), 'plugin',
                                    'flash.swf')),
        msg='Missing required flash data file.')
    url = self.GetFileURLForContentDataPath('plugin', 'flash.swf')
    self._RunNewTabTest('NewTabFlashPage', lambda: self._AppendTab(url),
                        'open_tab')

  def test20Tabs(self):
    """Measures time to open 20 tabs."""
    self._RunNewTabTest('20TabsNewTabPage',
                        lambda: self._AppendTab('chrome://newtab'),
                        'open_20_tabs', num_tabs=20)


class BenchmarkPerfTest(BasePerfTest):
  """Benchmark performance tests."""

  def testV8BenchmarkSuite(self):
    """Measures score from v8 benchmark suite."""
    url = self.GetFileURLForDataPath('v8_benchmark_v6', 'run.html')

    def _RunBenchmarkOnce(url):
      """Runs the v8 benchmark suite once and returns the results in a dict."""
      self.assertTrue(self.AppendTab(pyauto.GURL(url)),
                      msg='Failed to append tab for v8 benchmark suite.')
      js_done = """
          var val = document.getElementById("status").innerHTML;
          window.domAutomationController.send(val);
      """
      self.assertTrue(
          self.WaitUntil(
              lambda: 'Score:' in self.ExecuteJavascript(js_done, tab_index=1),
              timeout=300, expect_retval=True, retry_sleep=1),
          msg='Timed out when waiting for v8 benchmark score.')

      js_get_results = """
          var result = {};
          result['final_score'] = document.getElementById("status").innerHTML;
          result['all_results'] = document.getElementById("results").innerHTML;
          window.domAutomationController.send(JSON.stringify(result));
      """
      results = eval(self.ExecuteJavascript(js_get_results, tab_index=1))
      score_pattern = '(\w+): (\d+)'
      final_score = re.search(score_pattern, results['final_score']).group(2)
      result_dict = {'final_score': int(final_score)}
      for match in re.finditer(score_pattern, results['all_results']):
        benchmark_name = match.group(1)
        benchmark_score = match.group(2)
        result_dict[benchmark_name] = int(benchmark_score)
      self.CloseTab(tab_index=1)
      return result_dict

    timings = {}
    for iteration in xrange(self._num_iterations + 1):
      result_dict = _RunBenchmarkOnce(url)
      # Ignore the first iteration.
      if iteration:
        for key, val in result_dict.items():
          timings.setdefault(key, []).append(val)
        logging.info('Iteration %d of %d:\n%s', iteration,
                     self._num_iterations, self.pformat(result_dict))

    for key, val in timings.items():
      if key == 'final_score':
        self._PrintSummaryResults('V8Benchmark', val, 'score',
                                  'v8_benchmark_final')
      else:
        self._PrintSummaryResults('V8Benchmark-%s' % key, val, 'score',
                                  'v8_benchmark_individual')

  def testSunSpider(self):
    """Runs the SunSpider javascript benchmark suite."""
    url = self.GetFileURLForDataPath('sunspider', 'sunspider-driver.html')
    self.assertTrue(self.AppendTab(pyauto.GURL(url)),
                    msg='Failed to append tab for SunSpider benchmark suite.')

    js_is_done = """
        var done = false;
        if (document.getElementById("console"))
          done = true;
        window.domAutomationController.send(JSON.stringify(done));
    """
    self.assertTrue(
        self.WaitUntil(
            lambda: self.ExecuteJavascript(js_is_done, tab_index=1),
            timeout=300, expect_retval='true', retry_sleep=1),
        msg='Timed out when waiting for SunSpider benchmark score.')

    js_get_results = """
        window.domAutomationController.send(
            document.getElementById("console").innerHTML);
    """
    # Append '<br>' to the result to simplify regular expression matching.
    results = self.ExecuteJavascript(js_get_results, tab_index=1) + '<br>'
    total = re.search('Total:\s*([\d.]+)ms', results).group(1)
    logging.info('Total: %f ms', float(total))
    self._OutputPerfGraphValue('SunSpider-total', float(total), 'ms',
                               'sunspider_total')

    for match_category in re.finditer('\s\s(\w+):\s*([\d.]+)ms.+?<br><br>',
                                      results):
      category_name = match_category.group(1)
      category_result = match_category.group(2)
      logging.info('Benchmark "%s": %f ms', category_name,
                   float(category_result))
      self._OutputPerfGraphValue('SunSpider-' + category_name,
                                 float(category_result), 'ms',
                                 'sunspider_individual')

      for match_result in re.finditer('<br>\s\s\s\s([\w-]+):\s*([\d.]+)ms',
                                      match_category.group(0)):
        result_name = match_result.group(1)
        result_value = match_result.group(2)
        logging.info('  Result "%s-%s": %f ms', category_name, result_name,
                     float(result_value))
        self._OutputPerfGraphValue(
            'SunSpider-%s-%s' % (category_name, result_name),
            float(result_value), 'ms', 'sunspider_individual')

  def testDromaeoSuite(self):
    """Measures results from Dromaeo benchmark suite."""
    url = self.GetFileURLForDataPath('dromaeo', 'index.html')
    self.assertTrue(self.AppendTab(pyauto.GURL(url + '?dromaeo')),
                    msg='Failed to append tab for Dromaeo benchmark suite.')

    js_is_ready = """
        var val = document.getElementById('pause').value;
        window.domAutomationController.send(val);
    """
    self.assertTrue(
        self.WaitUntil(
            lambda: self.ExecuteJavascript(js_is_ready, tab_index=1),
            timeout=30, expect_retval='Run', retry_sleep=1),
        msg='Timed out when waiting for Dromaeo benchmark to load.')

    js_run = """
        $('#pause').val('Run').click();
        window.domAutomationController.send('done');
    """
    self.ExecuteJavascript(js_run, tab_index=1)

    js_is_done = """
        var val = document.getElementById('timebar').innerHTML;
        window.domAutomationController.send(val);
    """
    self.assertTrue(
        self.WaitUntil(
            lambda: 'Total' in self.ExecuteJavascript(js_is_done, tab_index=1),
            timeout=900, expect_retval=True, retry_sleep=2),
        msg='Timed out when waiting for Dromaeo benchmark to complete.')

    js_get_results = """
        var result = {};
        result['total_result'] = $('#timebar strong').html();
        result['all_results'] = {};
        $('.result-item.done').each(function (i) {
            var group_name = $(this).find('.test b').html().replace(':', '');
            var group_results = {};
            group_results['result'] =
                $(this).find('span').html().replace('runs/s', '')

            group_results['sub_groups'] = {}
            $(this).find('li').each(function (i) {
                var sub_name = $(this).find('b').html().replace(':', '');
                group_results['sub_groups'][sub_name] =
                    $(this).text().match(/: ([\d.]+)/)[1]
            });
            result['all_results'][group_name] = group_results;
        });
        window.domAutomationController.send(JSON.stringify(result));
    """
    results = eval(self.ExecuteJavascript(js_get_results, tab_index=1))
    total_result = results['total_result']
    logging.info('Total result: ' + total_result)
    self._OutputPerfGraphValue('Dromaeo-total', float(total_result),
                               'runsPerSec', 'dromaeo_total')

    for group_name, group in results['all_results'].iteritems():
      logging.info('Benchmark "%s": %s', group_name, group['result'])
      self._OutputPerfGraphValue('Dromaeo-' + group_name.replace(' ', ''),
                                 float(group['result']), 'runsPerSec',
                                 'dromaeo_individual')
      for benchmark_name, benchmark_score in group['sub_groups'].iteritems():
        logging.info('  Result "%s": %s', benchmark_name, benchmark_score)

  def testSpaceport(self):
    """Measures results from Spaceport benchmark suite."""
    # TODO(tonyg): Test is failing on bots. Diagnose and re-enable.
    pass

#    url = self.GetFileURLForDataPath('third_party', 'spaceport', 'index.html')
#    self.assertTrue(self.AppendTab(pyauto.GURL(url + '?auto')),
#                    msg='Failed to append tab for Spaceport benchmark suite.')
#
#    # The test reports results to console.log in the format "name: value".
#    # Inject a bit of JS to intercept those.
#    js_collect_console_log = """
#        window.__pyautoresult = {};
#        window.console.log = function(str) {
#            if (!str) return;
#            var key_val = str.split(': ');
#            if (!key_val.length == 2) return;
#            __pyautoresult[key_val[0]] = key_val[1];
#        };
#        window.domAutomationController.send('done');
#    """
#    self.ExecuteJavascript(js_collect_console_log, tab_index=1)
#
#    def _IsDone():
#      expected_num_results = 30  # The number of tests in benchmark.
#      results = eval(self.ExecuteJavascript(js_get_results, tab_index=1))
#      return expected_num_results == len(results)
#
#    js_get_results = """
#        window.domAutomationController.send(
#            JSON.stringify(window.__pyautoresult));
#    """
#    self.assertTrue(
#        self.WaitUntil(_IsDone, timeout=1200, expect_retval=True,
#                       retry_sleep=5),
#        msg='Timed out when waiting for Spaceport benchmark to complete.')
#    results = eval(self.ExecuteJavascript(js_get_results, tab_index=1))
#
#    for key in results:
#      suite, test = key.split('.')
#      value = float(results[key])
#      self._OutputPerfGraphValue(test, value, 'ObjectsAt30FPS', suite)
#    self._PrintSummaryResults('Overall', [float(x) for x in results.values()],
#                              'ObjectsAt30FPS', 'Overall')


class LiveWebappLoadTest(BasePerfTest):
  """Tests that involve performance measurements of live webapps.

  These tests connect to live webpages (e.g., Gmail, Calendar, Docs) and are
  therefore subject to network conditions.  These tests are meant to generate
  "ball-park" numbers only (to see roughly how long things take to occur from a
  user's perspective), and are not expected to be precise.
  """

  def testNewTabGmail(self):
    """Measures time to open a tab to a logged-in Gmail account.

    Timing starts right before the new tab is opened, and stops as soon as the
    webpage displays the substring 'Last account activity:'.
    """
    EXPECTED_SUBSTRING = 'Last account activity:'

    def _SubstringExistsOnPage():
      js = """
          var frame = document.getElementById("canvas_frame");
          var divs = frame.contentDocument.getElementsByTagName("div");
          for (var i = 0; i < divs.length; ++i) {
            if (divs[i].innerHTML.indexOf("%s") >= 0)
              window.domAutomationController.send("true");
          }
          window.domAutomationController.send("false");
      """ % EXPECTED_SUBSTRING
      return self.ExecuteJavascript(js, tab_index=1)

    def _RunSingleGmailTabOpen():
      self._AppendTab('http://www.gmail.com')
      self.assertTrue(self.WaitUntil(_SubstringExistsOnPage, timeout=120,
                                     expect_retval='true', retry_sleep=0.10),
                      msg='Timed out waiting for expected Gmail string.')

    self._LoginToGoogleAccount()
    self._RunNewTabTest('NewTabGmail', _RunSingleGmailTabOpen,
                        'open_tab_live_webapp')

  def testNewTabCalendar(self):
    """Measures time to open a tab to a logged-in Calendar account.

    Timing starts right before the new tab is opened, and stops as soon as the
    webpage displays the calendar print button (title 'Print my calendar').
    """
    EXPECTED_SUBSTRING = 'Month'

    def _DivTitleStartsWith():
      js = """
          var divs = document.getElementsByTagName("div");
          for (var i = 0; i < divs.length; ++i) {
            if (divs[i].innerHTML == "%s")
              window.domAutomationController.send("true");
          }
          window.domAutomationController.send("false");
      """ % EXPECTED_SUBSTRING
      return self.ExecuteJavascript(js, tab_index=1)

    def _RunSingleCalendarTabOpen():
      self._AppendTab('http://calendar.google.com')
      self.assertTrue(self.WaitUntil(_DivTitleStartsWith, timeout=120,
                                     expect_retval='true', retry_sleep=0.10),
                      msg='Timed out waiting for expected Calendar string.')

    self._LoginToGoogleAccount()
    self._RunNewTabTest('NewTabCalendar', _RunSingleCalendarTabOpen,
                        'open_tab_live_webapp')

  def testNewTabDocs(self):
    """Measures time to open a tab to a logged-in Docs account.

    Timing starts right before the new tab is opened, and stops as soon as the
    webpage displays the expected substring 'last modified' (case insensitive).
    """
    EXPECTED_SUBSTRING = 'sort'

    def _SubstringExistsOnPage():
      js = """
          var divs = document.getElementsByTagName("div");
          for (var i = 0; i < divs.length; ++i) {
            if (divs[i].innerHTML.toLowerCase().indexOf("%s") >= 0)
              window.domAutomationController.send("true");
          }
          window.domAutomationController.send("false");
      """ % EXPECTED_SUBSTRING
      return self.ExecuteJavascript(js, tab_index=1)

    def _RunSingleDocsTabOpen():
      self._AppendTab('http://docs.google.com')
      self.assertTrue(self.WaitUntil(_SubstringExistsOnPage, timeout=120,
                                     expect_retval='true', retry_sleep=0.10),
                      msg='Timed out waiting for expected Docs string.')

    self._LoginToGoogleAccount()
    self._RunNewTabTest('NewTabDocs', _RunSingleDocsTabOpen,
                        'open_tab_live_webapp')


class NetflixPerfTest(BasePerfTest, NetflixTestHelper):
  """Test Netflix video performance."""

  def __init__(self, methodName='runTest', **kwargs):
    pyauto.PyUITest.__init__(self, methodName, **kwargs)
    NetflixTestHelper.__init__(self, self)

  def tearDown(self):
    self.SignOut()
    pyauto.PyUITest.tearDown(self)

  def testNetflixDroppedFrames(self):
    """Measures the Netflix video dropped frames/second. Runs for 60 secs."""
    self.LoginAndStartPlaying()
    self.CheckNetflixPlaying(self.IS_PLAYING,
                             'Player did not start playing the title.')
    # Ignore first 10 seconds of video playing so we get smooth videoplayback.
    time.sleep(10)
    init_dropped_frames = self._GetVideoDroppedFrames()
    dropped_frames = []
    prev_dropped_frames = 0
    for iteration in xrange(60):
      # Ignoring initial dropped frames of first 10 seconds.
      total_dropped_frames = self._GetVideoDroppedFrames() - init_dropped_frames
      dropped_frames_last_sec = total_dropped_frames - prev_dropped_frames
      dropped_frames.append(dropped_frames_last_sec)
      logging.info('Iteration %d of %d: %f dropped frames in the last second',
                   iteration + 1, 60, dropped_frames_last_sec)
      prev_dropped_frames = total_dropped_frames
      # Play the video for some time.
      time.sleep(1)
    self._PrintSummaryResults('NetflixDroppedFrames', dropped_frames, 'frames',
                              'netflix_dropped_frames')

  def testNetflixCPU(self):
    """Measures the Netflix video CPU usage. Runs for 60 seconds."""
    self.LoginAndStartPlaying()
    self.CheckNetflixPlaying(self.IS_PLAYING,
                             'Player did not start playing the title.')
    # Ignore first 10 seconds of video playing so we get smooth videoplayback.
    time.sleep(10)
    init_dropped_frames = self._GetVideoDroppedFrames()
    init_video_frames = self._GetVideoFrames()
    cpu_usage_start = self._GetCPUUsage()
    total_shown_frames = 0
    # Play the video for some time.
    time.sleep(60)
    total_video_frames = self._GetVideoFrames() - init_video_frames
    total_dropped_frames = self._GetVideoDroppedFrames() - init_dropped_frames
    cpu_usage_end = self._GetCPUUsage()
    fraction_non_idle_time = \
        self._GetFractionNonIdleCPUTime(cpu_usage_start, cpu_usage_end)
    # Counting extrapolation for utilization to play the video.
    extrapolation_value = fraction_non_idle_time * \
        (float(total_video_frames) + total_dropped_frames) / total_video_frames
    logging.info('Netflix CPU extrapolation: %f', extrapolation_value)
    self._OutputPerfGraphValue('NetflixCPUExtrapolation', extrapolation_value,
                               'extrapolation', 'netflix_cpu_extrapolation')


class YoutubePerfTest(BasePerfTest, YoutubeTestHelper):
  """Test Youtube video performance."""

  def __init__(self, methodName='runTest', **kwargs):
    pyauto.PyUITest.__init__(self, methodName, **kwargs)
    YoutubeTestHelper.__init__(self, self)

  def _VerifyVideoTotalBytes(self):
    """Returns true if video total bytes information is available."""
    return self.GetVideoTotalBytes() > 0

  def _VerifyVideoLoadedBytes(self):
    """Returns true if video loaded bytes information is available."""
    return self.GetVideoLoadedBytes() > 0

  def StartVideoForPerformance(self, video_id='zuzaxlddWbk'):
    """Start the test video with all required buffering."""
    self.PlayVideoAndAssert(video_id)
    self.ExecuteJavascript("""
        ytplayer.setPlaybackQuality('hd720');
        window.domAutomationController.send('');
    """)
    self.AssertPlayerState(state=self.is_playing,
                           msg='Player did not enter the playing state')
    self.assertTrue(
        self.WaitUntil(self._VerifyVideoTotalBytes, expect_retval=True),
        msg='Failed to get video total bytes information.')
    self.assertTrue(
        self.WaitUntil(self._VerifyVideoLoadedBytes, expect_retval=True),
        msg='Failed to get video loaded bytes information')
    loaded_video_bytes = self.GetVideoLoadedBytes()
    total_video_bytes = self.GetVideoTotalBytes()
    self.PauseVideo()
    logging.info('total_video_bytes: %f', total_video_bytes)
    # Wait for the video to finish loading.
    while total_video_bytes > loaded_video_bytes:
      loaded_video_bytes = self.GetVideoLoadedBytes()
      logging.info('loaded_video_bytes: %f', loaded_video_bytes)
      time.sleep(1)
    self.PlayVideo()
    # Ignore first 10 seconds of video playing so we get smooth videoplayback.
    time.sleep(10)

  def testYoutubeDroppedFrames(self):
    """Measures the Youtube video dropped frames/second. Runs for 60 secs.

    This test measures Youtube video dropped frames for three different types
    of videos like slow, normal and fast motion.
    """
    youtube_video = {'Slow': 'VT1-sitWRtY',
                     'Normal': '2tqK_3mKQUw',
                     'Fast': '8ETDE0VGJY4',
                    }
    for video_type in youtube_video:
      logging.info('Running %s video.', video_type)
      self.StartVideoForPerformance(youtube_video[video_type])
      init_dropped_frames = self.GetVideoDroppedFrames()
      total_dropped_frames = 0
      dropped_fps = []
      for iteration in xrange(60):
        frames = self.GetVideoDroppedFrames() - init_dropped_frames
        current_dropped_frames = frames - total_dropped_frames
        dropped_fps.append(current_dropped_frames)
        logging.info('Iteration %d of %d: %f dropped frames in the last '
                     'second', iteration + 1, 60, current_dropped_frames)
        total_dropped_frames = frames
        # Play the video for some time
        time.sleep(1)
      graph_description = 'YoutubeDroppedFrames' + video_type
      self._PrintSummaryResults(graph_description, dropped_fps, 'frames',
                                'youtube_dropped_frames')

  def testYoutubeCPU(self):
    """Measures the Youtube video CPU usage. Runs for 60 seconds.

    Measures the Youtube video CPU usage (between 0 and 1), extrapolated to
    totalframes in the video by taking dropped frames into account. For smooth
    videoplayback this number should be < 0.5..1.0 on a hyperthreaded CPU.
    """
    self.StartVideoForPerformance()
    init_dropped_frames = self.GetVideoDroppedFrames()
    logging.info('init_dropped_frames: %f', init_dropped_frames)
    cpu_usage_start = self._GetCPUUsage()
    total_shown_frames = 0
    for sec_num in xrange(60):
      # Play the video for some time.
      time.sleep(1)
      total_shown_frames = total_shown_frames + self.GetVideoFrames()
      logging.info('total_shown_frames: %f', total_shown_frames)
    total_dropped_frames = self.GetVideoDroppedFrames() - init_dropped_frames
    logging.info('total_dropped_frames: %f', total_dropped_frames)
    cpu_usage_end = self._GetCPUUsage()
    fraction_non_idle_time = self._GetFractionNonIdleCPUTime(
        cpu_usage_start, cpu_usage_end)
    logging.info('fraction_non_idle_time: %f', fraction_non_idle_time)
    total_frames = total_shown_frames + total_dropped_frames
    # Counting extrapolation for utilization to play the video.
    extrapolation_value = (fraction_non_idle_time *
                           (float(total_frames) / total_shown_frames))
    logging.info('Youtube CPU extrapolation: %f', extrapolation_value)
    # Video is still running so log some more detailed data.
    self._LogProcessActivity()
    self._OutputPerfGraphValue('YoutubeCPUExtrapolation', extrapolation_value,
                               'extrapolation', 'youtube_cpu_extrapolation')


class FlashVideoPerfTest(BasePerfTest):
  """General flash video performance tests."""

  def FlashVideo1080P(self):
    """Measures total dropped frames and average FPS for a 1080p flash video.

    This is a temporary test to be run manually for now, needed to collect some
    performance statistics across different ChromeOS devices.
    """
    # Open up the test webpage; it's assumed the test will start automatically.
    webpage_url = 'http://www/~arscott/fl/FlashVideoTests.html'
    self.assertTrue(self.AppendTab(pyauto.GURL(webpage_url)),
                    msg='Failed to append tab for webpage.')

    # Wait until the test is complete.
    js_is_done = """
        window.domAutomationController.send(JSON.stringify(tests_done));
    """
    self.assertTrue(
        self.WaitUntil(
            lambda: self.ExecuteJavascript(js_is_done, tab_index=1) == 'true',
            timeout=300, expect_retval=True, retry_sleep=1),
        msg='Timed out when waiting for test result.')

    # Retrieve and output the test results.
    js_results = """
        window.domAutomationController.send(JSON.stringify(tests_results));
    """
    test_result = eval(self.ExecuteJavascript(js_results, tab_index=1))
    test_result[0] = test_result[0].replace('true', 'True')
    test_result = eval(test_result[0])  # Webpage only does 1 test right now.

    description = 'FlashVideo1080P'
    result = test_result['averageFPS']
    logging.info('Result for %s: %f FPS (average)', description, result)
    self._OutputPerfGraphValue(description, result, 'FPS',
                               'flash_video_1080p_fps')
    result = test_result['droppedFrames']
    logging.info('Result for %s: %f dropped frames', description, result)
    self._OutputPerfGraphValue(description, result, 'DroppedFrames',
                               'flash_video_1080p_dropped_frames')


class WebGLTest(BasePerfTest):
  """Tests for WebGL performance."""

  def _RunWebGLTest(self, url, description, graph_name):
    """Measures FPS using a specified WebGL demo.

    Args:
      url: The string URL that, once loaded, will run the WebGL demo (default
          WebGL demo settings are used, since this test does not modify any
          settings in the demo).
      description: A string description for this demo, used as a performance
          value description.  Should not contain any spaces.
      graph_name: A string name for the performance graph associated with this
          test.  Only used on Chrome desktop.
    """
    self.assertTrue(self.AppendTab(pyauto.GURL(url)),
                    msg='Failed to append tab for %s.' % description)

    get_fps_js = """
      var fps_field = document.getElementById("fps");
      var result = -1;
      if (fps_field)
        result = fps_field.innerHTML;
      window.domAutomationController.send(JSON.stringify(result));
    """

    # Wait until we start getting FPS values.
    self.assertTrue(
        self.WaitUntil(
            lambda: self.ExecuteJavascript(get_fps_js, tab_index=1) != '-1',
            timeout=300, retry_sleep=1),
        msg='Timed out when waiting for FPS values to be available.')

    # Let the experiment run for 5 seconds before we start collecting perf
    # measurements.
    time.sleep(5)

    # Collect the current FPS value each second for the next 30 seconds.  The
    # final result of this test will be the average of these FPS values.
    fps_vals = []
    for iteration in xrange(30):
      fps = self.ExecuteJavascript(get_fps_js, tab_index=1)
      fps = float(fps.replace('"', ''))
      fps_vals.append(fps)
      logging.info('Iteration %d of %d: %f FPS', iteration + 1, 30, fps)
      time.sleep(1)
    self._PrintSummaryResults(description, fps_vals, 'fps', graph_name)

  def testWebGLAquarium(self):
    """Measures performance using the WebGL Aquarium demo."""
    self._RunWebGLTest(
        self.GetFileURLForDataPath('pyauto_private', 'webgl', 'aquarium',
                                   'aquarium.html'),
        'WebGLAquarium', 'webgl_demo')

  def testWebGLField(self):
    """Measures performance using the WebGL Field demo."""
    self._RunWebGLTest(
        self.GetFileURLForDataPath('pyauto_private', 'webgl', 'field',
                                   'field.html'),
        'WebGLField', 'webgl_demo')

  def testWebGLSpaceRocks(self):
    """Measures performance using the WebGL SpaceRocks demo."""
    self._RunWebGLTest(
        self.GetFileURLForDataPath('pyauto_private', 'webgl', 'spacerocks',
                                   'spacerocks.html'),
        'WebGLSpaceRocks', 'webgl_demo')


class GPUPerfTest(BasePerfTest):
  """Tests for GPU performance."""

  def setUp(self):
    """Performs necessary setup work before running each test in this class."""
    self._gpu_info_dict = self.EvalDataFrom(os.path.join(self.DataDir(),
                                            'gpu', 'gpuperf.txt'))
    self._demo_name_url_dict = self._gpu_info_dict['demo_info']
    pyauto.PyUITest.setUp(self)

  def _MeasureFpsOverTime(self, tab_index=0):
    """Measures FPS using a specified demo.

    This function assumes that the demo is already loaded in the specified tab
    index.

    Args:
      tab_index: The tab index, default is 0.
    """
    # Let the experiment run for 5 seconds before we start collecting FPS
    # values.
    time.sleep(5)

    # Collect the current FPS value each second for the next 10 seconds.
    # Then return the average FPS value from among those collected.
    fps_vals = []
    for iteration in xrange(10):
      fps = self.GetFPS(tab_index=tab_index)
      fps_vals.append(fps['fps'])
      time.sleep(1)
    return Mean(fps_vals)

  def _GetStdAvgAndCompare(self, avg_fps, description, ref_dict):
    """Computes the average and compare set of values with reference data.

    Args:
      avg_fps: Average fps value.
      description: A string description for this demo, used as a performance
                   value description.
      ref_dict: Dictionary which contains reference data for this test case.

    Returns:
      True, if the actual FPS value is within 10% of the reference FPS value,
      or False, otherwise.
    """
    std_fps = 0
    status = True
    # Load reference data according to platform.
    platform_ref_dict = None
    if self.IsWin():
      platform_ref_dict = ref_dict['win']
    elif self.IsMac():
      platform_ref_dict = ref_dict['mac']
    elif self.IsLinux():
      platform_ref_dict = ref_dict['linux']
    else:
      self.assertFail(msg='This platform is unsupported.')
    std_fps = platform_ref_dict[description]
    # Compare reference data to average fps.
    # We allow the average FPS value to be within 10% of the reference
    # FPS value.
    if avg_fps < (0.9 * std_fps):
      logging.info('FPS difference exceeds threshold for: %s', description)
      logging.info('  Average: %f fps', avg_fps)
      logging.info('Reference Average: %f fps', std_fps)
      status = False
    else:
      logging.info('Average FPS is actually greater than 10 percent '
                   'more than the reference FPS for: %s', description)
      logging.info('  Average: %f fps', avg_fps)
      logging.info('  Reference Average: %f fps', std_fps)
    return status

  def testLaunchDemosParallelInSeparateTabs(self):
    """Measures performance of demos in different tabs in same browser."""
    # Launch all the demos parallel in separate tabs
    counter = 0
    all_demos_passed = True
    ref_dict = self._gpu_info_dict['separate_tab_ref_data']
    # Iterate through dictionary and append all url to browser
    for url in self._demo_name_url_dict.iterkeys():
      self.assertTrue(
          self.AppendTab(pyauto.GURL(self._demo_name_url_dict[url])),
          msg='Failed to append tab for %s.' % url)
      counter += 1
      # Assert number of tab count is equal to number of tabs appended.
      self.assertEqual(self.GetTabCount(), counter + 1)
      # Measures performance using different demos and compare it golden
      # reference.
    for url in self._demo_name_url_dict.iterkeys():
      avg_fps = self._MeasureFpsOverTime(tab_index=counter)
      # Get the reference value of fps and compare the results
      if not self._GetStdAvgAndCompare(avg_fps, url, ref_dict):
        all_demos_passed = False
      counter -= 1
    self.assertTrue(
        all_demos_passed,
        msg='One or more demos failed to yield an acceptable FPS value')

  def testLaunchDemosInSeparateBrowser(self):
    """Measures performance by launching each demo in a separate tab."""
    # Launch demos in the browser
    ref_dict = self._gpu_info_dict['separate_browser_ref_data']
    all_demos_passed = True
    for url in self._demo_name_url_dict.iterkeys():
      self.NavigateToURL(self._demo_name_url_dict[url])
      # Measures performance using different demos.
      avg_fps = self._MeasureFpsOverTime()
      self.RestartBrowser()
      # Get the standard value of fps and compare the rseults
      if not self._GetStdAvgAndCompare(avg_fps, url, ref_dict):
        all_demos_passed = False
    self.assertTrue(
        all_demos_passed,
        msg='One or more demos failed to yield an acceptable FPS value')

  def testLaunchDemosBrowseForwardBackward(self):
    """Measures performance of various demos in browser going back and forth."""
    ref_dict = self._gpu_info_dict['browse_back_forward_ref_data']
    url_array = []
    desc_array = []
    all_demos_passed = True
    # Get URL/Description from dictionary and put in individual array
    for url in self._demo_name_url_dict.iterkeys():
      url_array.append(self._demo_name_url_dict[url])
      desc_array.append(url)
    for index in range(len(url_array) - 1):
      # Launch demo in the Browser
      if index == 0:
        self.NavigateToURL(url_array[index])
        # Measures performance using the first demo.
        avg_fps = self._MeasureFpsOverTime()
        status1 = self._GetStdAvgAndCompare(avg_fps, desc_array[index],
                                            ref_dict)
      # Measures performance using the second demo.
      self.NavigateToURL(url_array[index + 1])
      avg_fps = self._MeasureFpsOverTime()
      status2 = self._GetStdAvgAndCompare(avg_fps, desc_array[index + 1],
                                          ref_dict)
      # Go Back to previous demo
      self.TabGoBack()
      # Measures performance for first demo when moved back
      avg_fps = self._MeasureFpsOverTime()
      status3 = self._GetStdAvgAndCompare(
          avg_fps, desc_array[index] + '_backward',
          ref_dict)
      # Go Forward to previous demo
      self.TabGoForward()
      # Measures performance for second demo when moved forward
      avg_fps = self._MeasureFpsOverTime()
      status4 = self._GetStdAvgAndCompare(
          avg_fps, desc_array[index + 1] + '_forward',
          ref_dict)
      if not all([status1, status2, status3, status4]):
        all_demos_passed = False
    self.assertTrue(
        all_demos_passed,
        msg='One or more demos failed to yield an acceptable FPS value')


class HTML5BenchmarkTest(BasePerfTest):
  """Tests for HTML5 performance."""

  def testHTML5Benchmark(self):
    """Measures performance using the benchmark at html5-benchmark.com."""
    self.NavigateToURL('http://html5-benchmark.com')

    start_benchmark_js = """
      benchmark();
      window.domAutomationController.send("done");
    """
    self.ExecuteJavascript(start_benchmark_js)

    js_final_score = """
      var score = "-1";
      var elem = document.getElementById("score");
      if (elem)
        score = elem.innerHTML;
      window.domAutomationController.send(score);
    """
    # Wait for the benchmark to complete, which is assumed to be when the value
    # of the 'score' DOM element changes to something other than '87485'.
    self.assertTrue(
        self.WaitUntil(
            lambda: self.ExecuteJavascript(js_final_score) != '87485',
            timeout=900, retry_sleep=1),
        msg='Timed out when waiting for final score to be available.')

    score = self.ExecuteJavascript(js_final_score)
    logging.info('HTML5 Benchmark final score: %f', float(score))
    self._OutputPerfGraphValue('HTML5Benchmark', float(score), 'score',
                               'html5_benchmark')


class FileUploadDownloadTest(BasePerfTest):
  """Tests that involve measuring performance of upload and download."""

  def setUp(self):
    """Performs necessary setup work before running each test in this class."""
    self._temp_dir = tempfile.mkdtemp()
    self._test_server = PerfTestServer(self._temp_dir)
    self._test_server_port = self._test_server.GetPort()
    self._test_server.Run()
    self.assertTrue(self.WaitUntil(self._IsTestServerRunning),
                    msg='Failed to start local performance test server.')
    BasePerfTest.setUp(self)

  def tearDown(self):
    """Performs necessary cleanup work after running each test in this class."""
    BasePerfTest.tearDown(self)
    self._test_server.ShutDown()
    pyauto_utils.RemovePath(self._temp_dir)

  def _IsTestServerRunning(self):
    """Determines whether the local test server is ready to accept connections.

    Returns:
      True, if a connection can be made to the local performance test server, or
      False otherwise.
    """
    conn = None
    try:
      conn = urllib2.urlopen('http://localhost:%d' % self._test_server_port)
      return True
    except IOError, e:
      return False
    finally:
      if conn:
        conn.close()

  def testDownload100MBFile(self):
    """Measures the time to download a 100 MB file from a local server."""
    CREATE_100MB_URL = (
        'http://localhost:%d/create_file_of_size?filename=data&mb=100' %
        self._test_server_port)
    DOWNLOAD_100MB_URL = 'http://localhost:%d/data' % self._test_server_port
    DELETE_100MB_URL = ('http://localhost:%d/delete_file?filename=data' %
                        self._test_server_port)

    # Tell the local server to create a 100 MB file.
    self.NavigateToURL(CREATE_100MB_URL)

    # Cleaning up downloaded files is done in the same way as in downloads.py.
    # We first identify all existing downloaded files, then remove only those
    # new downloaded files that appear during the course of this test.
    download_dir = self.GetDownloadDirectory().value()
    orig_downloads = []
    if os.path.isdir(download_dir):
      orig_downloads = os.listdir(download_dir)

    def _CleanupAdditionalFilesInDir(directory, orig_files):
      """Removes the additional files in the specified directory.

      This function will remove all files from |directory| that are not
      specified in |orig_files|.

      Args:
        directory: A string directory path.
        orig_files: A list of strings representing the original set of files in
            the specified directory.
      """
      downloads_to_remove = []
      if os.path.isdir(directory):
        downloads_to_remove = [os.path.join(directory, name)
                               for name in os.listdir(directory)
                               if name not in orig_files]
      for file_name in downloads_to_remove:
        pyauto_utils.RemovePath(file_name)

    def _DownloadFile(url):
      self.DownloadAndWaitForStart(url)
      self.WaitForAllDownloadsToComplete(timeout=2 * 60 * 1000)  # 2 minutes.

    timings = []
    for iteration in range(self._num_iterations + 1):
      elapsed_time = self._MeasureElapsedTime(
          lambda: _DownloadFile(DOWNLOAD_100MB_URL), num_invocations=1)
      # Ignore the first iteration.
      if iteration:
        timings.append(elapsed_time)
        logging.info('Iteration %d of %d: %f milliseconds', iteration,
                     self._num_iterations, elapsed_time)
      self.SetDownloadShelfVisible(False)
      _CleanupAdditionalFilesInDir(download_dir, orig_downloads)

    self._PrintSummaryResults('Download100MBFile', timings, 'milliseconds',
                              'download_file')

    # Tell the local server to delete the 100 MB file.
    self.NavigateToURL(DELETE_100MB_URL)

  def testUpload50MBFile(self):
    """Measures the time to upload a 50 MB file to a local server."""
    # TODO(dennisjeffrey): Replace the use of XMLHttpRequest in this test with
    # FileManager automation to select the upload file when crosbug.com/17903
    # is complete.
    START_UPLOAD_URL = (
        'http://localhost:%d/start_upload?mb=50' % self._test_server_port)

    EXPECTED_SUBSTRING = 'Upload complete'

    def _IsUploadComplete():
      js = """
          result = "";
          var div = document.getElementById("upload_result");
          if (div)
            result = div.innerHTML;
          window.domAutomationController.send(result);
      """
      return self.ExecuteJavascript(js).find(EXPECTED_SUBSTRING) >= 0

    def _RunSingleUpload():
      self.NavigateToURL(START_UPLOAD_URL)
      self.assertTrue(
          self.WaitUntil(_IsUploadComplete, timeout=120, expect_retval=True,
                         retry_sleep=0.10),
          msg='Upload failed to complete before the timeout was hit.')

    timings = []
    for iteration in range(self._num_iterations + 1):
      elapsed_time = self._MeasureElapsedTime(_RunSingleUpload)
      # Ignore the first iteration.
      if iteration:
        timings.append(elapsed_time)
        logging.info('Iteration %d of %d: %f milliseconds', iteration,
                     self._num_iterations, elapsed_time)

    self._PrintSummaryResults('Upload50MBFile', timings, 'milliseconds',
                              'upload_file')


class ScrollResults(object):
  """Container for ScrollTest results."""

  def __init__(self, first_paint_seconds, results_list):
    assert len(results_list) == 2, 'Expecting initial and repeat results.'
    self._first_paint_time = 1000.0 * first_paint_seconds
    self._results_list = results_list

  def GetFirstPaintTime(self):
    return self._first_paint_time

  def GetFrameCount(self, index):
    results = self._results_list[index]
    return results.get('numFramesSentToScreen', results['numAnimationFrames'])

  def GetFps(self, index):
    return (self.GetFrameCount(index) /
            self._results_list[index]['totalTimeInSeconds'])

  def GetMeanFrameTime(self, index):
    return (self._results_list[index]['totalTimeInSeconds'] /
            self.GetFrameCount(index))

  def GetPercentBelow60Fps(self, index):
    return (float(self._results_list[index]['droppedFrameCount']) /
            self.GetFrameCount(index))


class BaseScrollTest(BasePerfTest):
  """Base class for tests measuring scrolling performance."""

  def setUp(self):
    """Performs necessary setup work before running each test."""
    super(BaseScrollTest, self).setUp()
    scroll_file = os.path.join(self.DataDir(), 'scroll', 'scroll.js')
    with open(scroll_file) as f:
      self._scroll_text = f.read()

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Extra flag used by scroll performance tests.
    return (super(BaseScrollTest, self).ExtraChromeFlags() +
            ['--enable-gpu-benchmarking'])

  def RunSingleInvocation(self, url, is_gmail_test=False):
    """Runs a single invocation of the scroll test.

    Args:
      url: The string url for the webpage on which to run the scroll test.
      is_gmail_test: True iff the test is a GMail test.

    Returns:
      Instance of ScrollResults.
    """

    self.assertTrue(self.AppendTab(pyauto.GURL(url)),
                    msg='Failed to append tab for webpage.')

    timeout = pyauto.PyUITest.ActionTimeoutChanger(self, 300 * 1000)  # ms
    test_js = """%s;
        new __ScrollTest(function(results) {
          var stringify = JSON.stringify || JSON.encode;
          window.domAutomationController.send(stringify(results));
        }, %s);
    """ % (self._scroll_text, 'true' if is_gmail_test else 'false')
    results = simplejson.loads(self.ExecuteJavascript(test_js, tab_index=1))

    first_paint_js = ('window.domAutomationController.send('
                      '(chrome.loadTimes().firstPaintTime - '
                      'chrome.loadTimes().requestTime).toString());')
    first_paint_time = float(self.ExecuteJavascript(first_paint_js,
                                                    tab_index=1))

    self.CloseTab(tab_index=1)

    return ScrollResults(first_paint_time, results)

  def RunScrollTest(self, url, description, graph_name, is_gmail_test=False):
    """Runs a scroll performance test on the specified webpage.

    Args:
      url: The string url for the webpage on which to run the scroll test.
      description: A string description for the particular test being run.
      graph_name: A string name for the performance graph associated with this
          test.  Only used on Chrome desktop.
      is_gmail_test: True iff the test is a GMail test.
    """
    results = []
    for iteration in range(self._num_iterations + 1):
      result = self.RunSingleInvocation(url, is_gmail_test)
      # Ignore the first iteration.
      if iteration:
        fps = result.GetFps(1)
        assert fps, '%s did not scroll' % url
        logging.info('Iteration %d of %d: %f fps', iteration,
                     self._num_iterations, fps)
        results.append(result)
    self._PrintSummaryResults(
        description, [r.GetFps(1) for r in results],
        'FPS', graph_name)


class PopularSitesScrollTest(BaseScrollTest):
  """Measures scrolling performance on recorded versions of popular sites."""

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    return super(PopularSitesScrollTest,
                 self).ExtraChromeFlags() + PageCyclerReplay.CHROME_FLAGS

  def _GetUrlList(self, test_name):
    """Returns list of recorded sites."""
    sites_path = PageCyclerReplay.Path('page_sets', test_name=test_name)
    with open(sites_path) as f:
      sites_text = f.read()
    js = """
      %s
      window.domAutomationController.send(JSON.stringify(pageSets));
    """ % sites_text
    page_sets = eval(self.ExecuteJavascript(js))
    return list(itertools.chain(*page_sets))[1:]  # Skip first.

  def _PrintScrollResults(self, results):
    self._PrintSummaryResults(
        'initial', [r.GetMeanFrameTime(0) for r in results],
        'ms', 'FrameTimes')
    self._PrintSummaryResults(
        'repeat', [r.GetMeanFrameTime(1) for r in results],
        'ms', 'FrameTimes')
    self._PrintSummaryResults(
        'initial',
        [r.GetPercentBelow60Fps(0) for r in results],
        'percent', 'PercentBelow60FPS')
    self._PrintSummaryResults(
        'repeat',
        [r.GetPercentBelow60Fps(1) for r in results],
        'percent', 'PercentBelow60FPS')
    self._PrintSummaryResults(
        'first_paint_time', [r.GetFirstPaintTime() for r in results],
        'ms', 'FirstPaintTime')

  def test2012Q3(self):
    test_name = '2012Q3'
    urls = self._GetUrlList(test_name)
    results = []
    with PageCyclerReplay.ReplayServer(test_name) as replay_server:
      if replay_server.is_record_mode:
        self._num_iterations = 1
      for iteration in range(self._num_iterations):
        for url in urls:
          result = self.RunSingleInvocation(url)
          fps = result.GetFps(0)
          assert fps, '%s did not scroll' % url
          logging.info('Iteration %d of %d: %f fps', iteration + 1,
                       self._num_iterations, fps)
          results.append(result)
    self._PrintScrollResults(results)


class ScrollTest(BaseScrollTest):
  """Tests to measure scrolling performance."""

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Extra flag needed by scroll performance tests.
    return super(ScrollTest, self).ExtraChromeFlags() + ['--disable-gpu-vsync']

  def testBlankPageScroll(self):
    """Runs the scroll test on a blank page."""
    self.RunScrollTest(
        self.GetFileURLForDataPath('scroll', 'blank.html'), 'ScrollBlankPage',
        'scroll_fps')

  def testTextScroll(self):
    """Runs the scroll test on a text-filled page."""
    self.RunScrollTest(
        self.GetFileURLForDataPath('scroll', 'text.html'), 'ScrollTextPage',
        'scroll_fps')

  def testGooglePlusScroll(self):
    """Runs the scroll test on a Google Plus anonymized page."""
    self.RunScrollTest(
        self.GetFileURLForDataPath('scroll', 'plus.html'),
        'ScrollGooglePlusPage', 'scroll_fps')

  def testGmailScroll(self):
    """Runs the scroll test using the live Gmail site."""
    self._LoginToGoogleAccount(account_key='test_google_account_gmail')
    self.RunScrollTest('http://www.gmail.com', 'ScrollGmail',
                       'scroll_fps', True)


class FlashTest(BasePerfTest):
  """Tests to measure flash performance."""

  def _RunFlashTestForAverageFPS(self, webpage_url, description, graph_name):
    """Runs a single flash test that measures an average FPS value.

    Args:
      webpage_url: The string URL to a webpage that will run the test.
      description: A string description for this test.
      graph_name: A string name for the performance graph associated with this
          test.  Only used on Chrome desktop.
    """
    # Open up the test webpage; it's assumed the test will start automatically.
    self.assertTrue(self.AppendTab(pyauto.GURL(webpage_url)),
                    msg='Failed to append tab for webpage.')

    # Wait until the final result is computed, then retrieve and output it.
    js = """
        window.domAutomationController.send(
            JSON.stringify(final_average_fps));
    """
    self.assertTrue(
        self.WaitUntil(
            lambda: self.ExecuteJavascript(js, tab_index=1) != '-1',
            timeout=300, expect_retval=True, retry_sleep=1),
        msg='Timed out when waiting for test result.')
    result = float(self.ExecuteJavascript(js, tab_index=1))
    logging.info('Result for %s: %f FPS (average)', description, result)
    self._OutputPerfGraphValue(description, result, 'FPS', graph_name)

  def testFlashGaming(self):
    """Runs a simple flash gaming benchmark test."""
    webpage_url = self.GetHttpURLForDataPath('pyauto_private', 'flash',
                                             'FlashGamingTest2.html')
    self._RunFlashTestForAverageFPS(webpage_url, 'FlashGaming', 'flash_fps')

  def testFlashText(self):
    """Runs a simple flash text benchmark test."""
    webpage_url = self.GetHttpURLForDataPath('pyauto_private', 'flash',
                                             'FlashTextTest2.html')
    self._RunFlashTestForAverageFPS(webpage_url, 'FlashText', 'flash_fps')

  def testScimarkGui(self):
    """Runs the ScimarkGui benchmark tests."""
    webpage_url = self.GetHttpURLForDataPath('pyauto_private', 'flash',
                                             'scimarkGui.html')
    self.assertTrue(self.AppendTab(pyauto.GURL(webpage_url)),
                    msg='Failed to append tab for webpage.')

    js = 'window.domAutomationController.send(JSON.stringify(tests_done));'
    self.assertTrue(
        self.WaitUntil(
            lambda: self.ExecuteJavascript(js, tab_index=1), timeout=300,
            expect_retval='true', retry_sleep=1),
        msg='Timed out when waiting for tests to complete.')

    js_result = """
        var result = {};
        for (var i = 0; i < tests_results.length; ++i) {
          var test_name = tests_results[i][0];
          var mflops = tests_results[i][1];
          var mem = tests_results[i][2];
          result[test_name] = [mflops, mem]
        }
        window.domAutomationController.send(JSON.stringify(result));
    """
    result = eval(self.ExecuteJavascript(js_result, tab_index=1))
    for benchmark in result:
      mflops = float(result[benchmark][0])
      mem = float(result[benchmark][1])
      if benchmark.endswith('_mflops'):
        benchmark = benchmark[:benchmark.find('_mflops')]
      logging.info('Results for ScimarkGui_%s:', benchmark)
      logging.info('  %f MFLOPS', mflops)
      logging.info('  %f MB', mem)
      self._OutputPerfGraphValue('ScimarkGui-%s-MFLOPS' % benchmark, mflops,
                                 'MFLOPS', 'scimark_gui_mflops')
      self._OutputPerfGraphValue('ScimarkGui-%s-Mem' % benchmark, mem, 'MB',
                                 'scimark_gui_mem')


class LiveGamePerfTest(BasePerfTest):
  """Tests to measure performance of live gaming webapps."""

  def _RunLiveGamePerfTest(self, url, url_title_substring,
                           description, graph_name):
    """Measures performance metrics for the specified live gaming webapp.

    This function connects to the specified URL to launch the gaming webapp,
    waits for a period of time for the webapp to run, then collects some
    performance metrics about the running webapp.

    Args:
      url: The string URL of the gaming webapp to analyze.
      url_title_substring: A string that is expected to be a substring of the
          webpage title for the specified gaming webapp.  Used to verify that
          the webapp loads correctly.
      description: A string description for this game, used in the performance
          value description.  Should not contain any spaces.
      graph_name: A string name for the performance graph associated with this
          test.  Only used on Chrome desktop.
    """
    self.NavigateToURL(url)
    loaded_tab_title = self.GetActiveTabTitle()
    self.assertTrue(url_title_substring in loaded_tab_title,
                    msg='Loaded tab title missing "%s": "%s"' %
                        (url_title_substring, loaded_tab_title))
    cpu_usage_start = self._GetCPUUsage()

    # Let the app run for 1 minute.
    time.sleep(60)

    cpu_usage_end = self._GetCPUUsage()
    fraction_non_idle_time = self._GetFractionNonIdleCPUTime(
        cpu_usage_start, cpu_usage_end)

    logging.info('Fraction of CPU time spent non-idle: %f',
                 fraction_non_idle_time)
    self._OutputPerfGraphValue(description + 'CpuBusy', fraction_non_idle_time,
                               'Fraction', graph_name + '_cpu_busy')
    v8_heap_stats = self.GetV8HeapStats()
    v8_heap_size = v8_heap_stats['v8_memory_used'] / (1024.0 * 1024.0)
    logging.info('Total v8 heap size: %f MB', v8_heap_size)
    self._OutputPerfGraphValue(description + 'V8HeapSize', v8_heap_size, 'MB',
                               graph_name + '_v8_heap_size')

  def testAngryBirds(self):
    """Measures performance for Angry Birds."""
    self._RunLiveGamePerfTest('http://chrome.angrybirds.com', 'Angry Birds',
                              'AngryBirds', 'angry_birds')


class BasePageCyclerTest(BasePerfTest):
  """Page class for page cycler tests.

  Derived classes must implement StartUrl().

  Environment Variables:
    PC_NO_AUTO: if set, avoids automatically loading pages.
  """
  MAX_ITERATION_SECONDS = 60
  TRIM_PERCENT = 20
  DEFAULT_USE_AUTO = True

  # Page Cycler lives in src/data/page_cycler rather than src/chrome/test/data
  DATA_PATH = os.path.abspath(
      os.path.join(BasePerfTest.DataDir(), os.pardir, os.pardir,
                   os.pardir, 'data', 'page_cycler'))

  def setUp(self):
    """Performs necessary setup work before running each test."""
    super(BasePageCyclerTest, self).setUp()
    self.use_auto = 'PC_NO_AUTO' not in os.environ

  @classmethod
  def DataPath(cls, subdir):
    return os.path.join(cls.DATA_PATH, subdir)

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Extra flags required to run these tests.
    # The first two are needed for the test.
    # The plugins argument is to prevent bad scores due to pop-ups from
    # running an old version of something (like Flash).
    return (super(BasePageCyclerTest, self).ExtraChromeFlags() +
            ['--js-flags="--expose_gc"',
             '--enable-file-cookies',
             '--allow-outdated-plugins'])

  def WaitUntilStarted(self, start_url):
    """Check that the test navigates away from the start_url."""
    js_is_started = """
        var is_started = document.location.href !== "%s";
        window.domAutomationController.send(JSON.stringify(is_started));
    """ % start_url
    self.assertTrue(
        self.WaitUntil(lambda: self.ExecuteJavascript(js_is_started) == 'true',
                       timeout=10),
        msg='Timed out when waiting to leave start page.')

  def WaitUntilDone(self, url, iterations):
    """Check cookies for "__pc_done=1" to know the test is over."""
    def IsDone():
      cookies = self.GetCookie(pyauto.GURL(url))  # window 0, tab 0
      return '__pc_done=1' in cookies
    self.assertTrue(
        self.WaitUntil(
            IsDone,
            timeout=(self.MAX_ITERATION_SECONDS * iterations),
            retry_sleep=1),
        msg='Timed out waiting for page cycler test to complete.')

  def CollectPagesAndTimes(self, url):
    """Collect the results from the cookies."""
    pages, times = None, None
    cookies = self.GetCookie(pyauto.GURL(url))  # window 0, tab 0
    for cookie in cookies.split(';'):
      if '__pc_pages' in cookie:
        pages_str = cookie.split('=', 1)[1]
        pages = pages_str.split(',')
      elif '__pc_timings' in cookie:
        times_str = cookie.split('=', 1)[1]
        times = [float(t) for t in times_str.split(',')]
    self.assertTrue(pages and times,
                    msg='Unable to find test results in cookies: %s' % cookies)
    return pages, times

  def IteratePageTimes(self, pages, times, iterations):
    """Regroup the times by the page.

    Args:
      pages: the list of pages
      times: e.g. [page1_iter1, page2_iter1, ..., page1_iter2, page2_iter2, ...]
      iterations: the number of times for each page
    Yields:
      (pageN, [pageN_iter1, pageN_iter2, ...])
    """
    num_pages = len(pages)
    num_times = len(times)
    expected_num_times = num_pages * iterations
    self.assertEqual(
        expected_num_times, num_times,
        msg=('num_times != num_pages * iterations: %s != %s * %s, times=%s' %
             (num_times, num_pages, iterations, times)))
    for i, page in enumerate(pages):
      yield page, list(itertools.islice(times, i, None, num_pages))

  def CheckPageTimes(self, pages, times, iterations):
    """Assert that all the times are greater than zero."""
    failed_pages = []
    for page, times in self.IteratePageTimes(pages, times, iterations):
      failed_times = [t for t in times if t <= 0.0]
      if failed_times:
        failed_pages.append((page, failed_times))
    if failed_pages:
      self.fail('Pages with unexpected times: %s' % failed_pages)

  def TrimTimes(self, times, percent):
    """Return a new list with |percent| number of times trimmed for each page.

    Removes the largest and smallest values.
    """
    iterations = len(times)
    times = sorted(times)
    num_to_trim = int(iterations * float(percent) / 100.0)
    logging.debug('Before trimming %d: %s' % (num_to_trim, times))
    a = num_to_trim / 2
    b = iterations - (num_to_trim / 2 + num_to_trim % 2)
    trimmed_times = times[a:b]
    logging.debug('After trimming: %s', trimmed_times)
    return trimmed_times

  def ComputeFinalResult(self, pages, times, iterations):
    """The final score that is calculated is a geometric mean of the
    arithmetic means of each page's load time, and we drop the
    upper/lower 20% of the times for each page so they don't skew the
    mean.  The geometric mean is used for the final score because the
    time range for any given site may be very different, and we don't
    want slower sites to weight more heavily than others.
    """
    self.CheckPageTimes(pages, times, iterations)
    page_means = [
        Mean(self.TrimTimes(times, percent=self.TRIM_PERCENT))
        for _, times in self.IteratePageTimes(pages, times, iterations)]
    return GeometricMean(page_means)

  def StartUrl(self, test_name, iterations):
    """Return the URL to used to start the test.

    Derived classes must implement this.
    """
    raise NotImplemented

  def RunPageCyclerTest(self, name, description):
    """Runs the specified PageCycler test.

    Args:
      name: the page cycler test name (corresponds to a directory or test file)
      description: a string description for the test
    """
    iterations = self._num_iterations
    start_url = self.StartUrl(name, iterations)
    self.NavigateToURL(start_url)
    if self.use_auto:
      self.WaitUntilStarted(start_url)
    self.WaitUntilDone(start_url, iterations)
    pages, times = self.CollectPagesAndTimes(start_url)
    final_result = self.ComputeFinalResult(pages, times, iterations)
    logging.info('%s page cycler final result: %f' %
                 (description, final_result))
    self._OutputPerfGraphValue(description + '_PageCycler', final_result,
                               'milliseconds', graph_name='PageCycler')


class PageCyclerTest(BasePageCyclerTest):
  """Tests to run various page cyclers.

  Environment Variables:
    PC_NO_AUTO: if set, avoids automatically loading pages.
  """

  def _PreReadDataDir(self, subdir):
    """This recursively reads all of the files in a given url directory.

    The intent is to get them into memory before they are used by the benchmark.

    Args:
      subdir: a subdirectory of the page cycler data directory.
    """
    def _PreReadDir(dirname, names):
      for rfile in names:
        with open(os.path.join(dirname, rfile)) as fp:
          fp.read()
    for root, dirs, files in os.walk(self.DataPath(subdir)):
      _PreReadDir(root, files)

  def StartUrl(self, test_name, iterations):
    # Must invoke GetFileURLForPath before appending parameters to the URL,
    # otherwise those parameters will get quoted.
    start_url = self.GetFileURLForPath(self.DataPath(test_name), 'start.html')
    start_url += '?iterations=%d' % iterations
    if self.use_auto:
      start_url += '&auto=1'
    return start_url

  def RunPageCyclerTest(self, dirname, description):
    """Runs the specified PageCycler test.

    Args:
      dirname: directory containing the page cycler test
      description: a string description for the test
    """
    self._PreReadDataDir('common')
    self._PreReadDataDir(dirname)
    super(PageCyclerTest, self).RunPageCyclerTest(dirname, description)

  def testMoreJSFile(self):
    self.RunPageCyclerTest('morejs', 'MoreJSFile')

  def testAlexaFile(self):
    self.RunPageCyclerTest('alexa_us', 'Alexa_usFile')

  def testBloatFile(self):
    self.RunPageCyclerTest('bloat', 'BloatFile')

  def testDHTMLFile(self):
    self.RunPageCyclerTest('dhtml', 'DhtmlFile')

  def testIntl1File(self):
    self.RunPageCyclerTest('intl1', 'Intl1File')

  def testIntl2File(self):
    self.RunPageCyclerTest('intl2', 'Intl2File')

  def testMozFile(self):
    self.RunPageCyclerTest('moz', 'MozFile')

  def testMoz2File(self):
    self.RunPageCyclerTest('moz2', 'Moz2File')


class PageCyclerReplay(object):
  """Run page cycler tests with network simulation via Web Page Replay.

  Web Page Replay is a proxy that can record and "replay" web pages with
  simulated network characteristics -- without having to edit the pages
  by hand. With WPR, tests can use "real" web content, and catch
  performance issues that may result from introducing network delays and
  bandwidth throttling.
  """
  _PATHS = {
      'archive':    'src/data/page_cycler/webpagereplay/{test_name}.wpr',
      'page_sets':  'src/tools/page_cycler/webpagereplay/tests/{test_name}.js',
      'start_page': 'src/tools/page_cycler/webpagereplay/start.html',
      'extension':  'src/tools/page_cycler/webpagereplay/extension',
      }

  WEBPAGEREPLAY_HOST = '127.0.0.1'
  WEBPAGEREPLAY_HTTP_PORT = 8080
  WEBPAGEREPLAY_HTTPS_PORT = 8413

  CHROME_FLAGS = webpagereplay.GetChromeFlags(
      WEBPAGEREPLAY_HOST,
      WEBPAGEREPLAY_HTTP_PORT,
      WEBPAGEREPLAY_HTTPS_PORT) + [
          '--log-level=0',
          '--disable-background-networking',
          '--enable-experimental-extension-apis',
          '--enable-logging',
          '--enable-benchmarking',
          '--metrics-recording-only',
          '--activate-on-launch',
          '--no-first-run',
          '--no-proxy-server',
          ]

  @classmethod
  def Path(cls, key, **kwargs):
    return FormatChromePath(cls._PATHS[key], **kwargs)

  @classmethod
  def ReplayServer(cls, test_name, replay_options=None):
    archive_path = cls.Path('archive', test_name=test_name)
    return webpagereplay.ReplayServer(archive_path,
                                      cls.WEBPAGEREPLAY_HOST,
                                      cls.WEBPAGEREPLAY_HTTP_PORT,
                                      cls.WEBPAGEREPLAY_HTTPS_PORT,
                                      replay_options)


class PageCyclerNetSimTest(BasePageCyclerTest):
  """Tests to run Web Page Replay backed page cycler tests."""
  MAX_ITERATION_SECONDS = 180

  def ExtraChromeFlags(self):
    """Ensures Chrome is launched with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    flags = super(PageCyclerNetSimTest, self).ExtraChromeFlags()
    flags.append('--load-extension=%s' % PageCyclerReplay.Path('extension'))
    flags.extend(PageCyclerReplay.CHROME_FLAGS)
    return flags

  def StartUrl(self, test_name, iterations):
    start_path = PageCyclerReplay.Path('start_page')
    start_url = 'file://%s?test=%s&iterations=%d' % (
        start_path, test_name, iterations)
    if self.use_auto:
      start_url += '&auto=1'
    return start_url

  def RunPageCyclerTest(self, test_name, description):
    """Runs the specified PageCycler test.

    Args:
      test_name: name for archive (.wpr) and config (.js) files.
      description: a string description for the test
    """
    replay_options = None
    with PageCyclerReplay.ReplayServer(test_name, replay_options) as server:
      if server.is_record_mode:
        self._num_iterations = 1
      super_self = super(PageCyclerNetSimTest, self)
      super_self.RunPageCyclerTest(test_name, description)

  def test2012Q2(self):
    self.RunPageCyclerTest('2012Q2', '2012Q2')


class MemoryTest(BasePerfTest):
  """Tests to measure memory consumption under different usage scenarios."""

  def ExtraChromeFlags(self):
    """Launches Chrome with custom flags.

    Returns:
      A list of extra flags to pass to Chrome when it is launched.
    """
    # Ensure Chrome assigns one renderer process to each tab.
    return super(MemoryTest, self).ExtraChromeFlags() + ['--process-per-tab']

  def _RecordMemoryStats(self, description, when, duration):
    """Outputs memory statistics to be graphed.

    Args:
      description: A string description for the test.  Should not contain
          spaces.  For example, 'MemCtrl'.
      when: A string description of when the memory stats are being recorded
          during test execution (since memory stats may be recorded multiple
          times during a test execution at certain "interesting" times).  Should
          not contain spaces.
      duration: The number of seconds to sample data before outputting the
          memory statistics.
    """
    mem = self.GetMemoryStatsChromeOS(duration)
    measurement_types = [
      ('gem_obj', 'GemObj'),
      ('gtt', 'GTT'),
      ('mem_free', 'MemFree'),
      ('mem_available', 'MemAvail'),
      ('mem_shared', 'MemShare'),
      ('mem_cached', 'MemCache'),
      ('mem_anon', 'MemAnon'),
      ('mem_file', 'MemFile'),
      ('mem_slab', 'MemSlab'),
      ('browser_priv', 'BrowPriv'),
      ('browser_shared', 'BrowShar'),
      ('gpu_priv', 'GpuPriv'),
      ('gpu_shared', 'GpuShar'),
      ('renderer_priv', 'RendPriv'),
      ('renderer_shared', 'RendShar'),
    ]
    for type_key, type_string in measurement_types:
      if type_key not in mem:
        continue
      self._OutputPerfGraphValue(
          '%s-Min%s-%s' % (description, type_string, when),
          mem[type_key]['min'], 'KB', '%s-%s' % (description, type_string))
      self._OutputPerfGraphValue(
          '%s-Max%s-%s' % (description, type_string, when),
          mem[type_key]['max'], 'KB', '%s-%s' % (description, type_string))
      self._OutputPerfGraphValue(
          '%s-End%s-%s' % (description, type_string, when),
          mem[type_key]['end'], 'KB', '%s-%s' % (description, type_string))

  def _RunTest(self, tabs, description, duration):
    """Runs a general memory test.

    Args:
      tabs: A list of strings representing the URLs of the websites to open
          during this test.
      description: A string description for the test.  Should not contain
          spaces.  For example, 'MemCtrl'.
      duration: The number of seconds to sample data before outputting memory
          statistics.
    """
    self._RecordMemoryStats(description, '0Tabs0', duration)

    for iteration_num in xrange(2):
      for site in tabs:
        self.AppendTab(pyauto.GURL(site))

      self._RecordMemoryStats(description,
                              '%dTabs%d' % (len(tabs), iteration_num + 1),
                              duration)

      for _ in xrange(len(tabs)):
        self.CloseTab(tab_index=1)

      self._RecordMemoryStats(description, '0Tabs%d' % (iteration_num + 1),
                              duration)

  def testOpenCloseTabsControl(self):
    """Measures memory usage when opening/closing tabs to about:blank."""
    tabs = ['about:blank'] * 10
    self._RunTest(tabs, 'MemCtrl', 15)

  def testOpenCloseTabsLiveSites(self):
    """Measures memory usage when opening/closing tabs to live sites."""
    tabs = [
      'http://www.google.com/gmail',
      'http://www.google.com/calendar',
      'http://www.google.com/plus',
      'http://www.google.com/youtube',
      'http://www.nytimes.com',
      'http://www.cnn.com',
      'http://www.facebook.com/zuck',
      'http://www.techcrunch.com',
      'http://www.theverge.com',
      'http://www.yahoo.com',
    ]
    # Log in to a test Google account to make connections to the above Google
    # websites more interesting.
    self._LoginToGoogleAccount()
    self._RunTest(tabs, 'MemLive', 20)


class PerfTestServerRequestHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
  """Request handler for the local performance test server."""

  def _IgnoreHandler(self, unused_args):
    """A GET request handler that simply replies with status code 200.

    Args:
      unused_args: A dictionary of arguments for the current GET request.
          The arguments are ignored.
    """
    self.send_response(200)
    self.end_headers()

  def _CreateFileOfSizeHandler(self, args):
    """A GET handler that creates a local file with the specified size.

    Args:
      args: A dictionary of arguments for the current GET request.  Must
          contain 'filename' and 'mb' keys that refer to the name of the file
          to create and its desired size, respectively.
    """
    megabytes = None
    filename = None
    try:
      megabytes = int(args['mb'][0])
      filename = args['filename'][0]
    except (ValueError, KeyError, IndexError), e:
      logging.exception('Server error creating file: %s', e)
    assert megabytes and filename
    with open(os.path.join(self.server.docroot, filename), 'wb') as f:
      f.write('X' * 1024 * 1024 * megabytes)
    self.send_response(200)
    self.end_headers()

  def _DeleteFileHandler(self, args):
    """A GET handler that deletes the specified local file.

    Args:
      args: A dictionary of arguments for the current GET request.  Must
          contain a 'filename' key that refers to the name of the file to
          delete, relative to the server's document root.
    """
    filename = None
    try:
      filename = args['filename'][0]
    except (KeyError, IndexError), e:
      logging.exception('Server error deleting file: %s', e)
    assert filename
    try:
      os.remove(os.path.join(self.server.docroot, filename))
    except OSError, e:
      logging.warning('OS error removing file: %s', e)
    self.send_response(200)
    self.end_headers()

  def _StartUploadHandler(self, args):
    """A GET handler to serve a page that uploads the given amount of data.

    When the page loads, the specified amount of data is automatically
    uploaded to the same local server that is handling the current request.

    Args:
      args: A dictionary of arguments for the current GET request.  Must
          contain an 'mb' key that refers to the size of the data to upload.
    """
    megabytes = None
    try:
      megabytes = int(args['mb'][0])
    except (ValueError, KeyError, IndexError), e:
      logging.exception('Server error starting upload: %s', e)
    assert megabytes
    script = """
        <html>
          <head>
            <script type='text/javascript'>
              function startUpload() {
                var megabytes = %s;
                var data = Array((1024 * 1024 * megabytes) + 1).join('X');
                var boundary = '***BOUNDARY***';
                var xhr = new XMLHttpRequest();

                xhr.open('POST', 'process_upload', true);
                xhr.setRequestHeader(
                    'Content-Type',
                    'multipart/form-data; boundary="' + boundary + '"');
                xhr.setRequestHeader('Content-Length', data.length);
                xhr.onreadystatechange = function() {
                  if (xhr.readyState == 4 && xhr.status == 200) {
                    document.getElementById('upload_result').innerHTML =
                        xhr.responseText;
                  }
                };
                var body = '--' + boundary + '\\r\\n';
                body += 'Content-Disposition: form-data;' +
                        'file_contents=' + data;
                xhr.send(body);
              }
            </script>
          </head>

          <body onload="startUpload();">
            <div id='upload_result'>Uploading...</div>
          </body>
        </html>
    """ % megabytes
    self.send_response(200)
    self.end_headers()
    self.wfile.write(script)

  def _ProcessUploadHandler(self, form):
    """A POST handler that discards uploaded data and sends a response.

    Args:
      form: A dictionary containing posted form data, as returned by
          urlparse.parse_qs().
    """
    upload_processed = False
    file_size = 0
    if 'file_contents' in form:
      file_size = len(form['file_contents'][0])
      upload_processed = True
    self.send_response(200)
    self.end_headers()
    if upload_processed:
      self.wfile.write('Upload complete (%d bytes)' % file_size)
    else:
      self.wfile.write('No file contents uploaded')

  GET_REQUEST_HANDLERS = {
    'create_file_of_size': _CreateFileOfSizeHandler,
    'delete_file': _DeleteFileHandler,
    'start_upload': _StartUploadHandler,
    'favicon.ico': _IgnoreHandler,
  }

  POST_REQUEST_HANDLERS = {
    'process_upload': _ProcessUploadHandler,
  }

  def translate_path(self, path):
    """Ensures files are served from the given document root.

    Overridden from SimpleHTTPServer.SimpleHTTPRequestHandler.
    """
    path = urlparse.urlparse(path)[2]
    path = posixpath.normpath(urllib.unquote(path))
    words = path.split('/')
    words = filter(None, words)  # Remove empty strings from |words|.
    path = self.server.docroot
    for word in words:
      _, word = os.path.splitdrive(word)
      _, word = os.path.split(word)
      if word in (os.curdir, os.pardir):
        continue
      path = os.path.join(path, word)
    return path

  def do_GET(self):
    """Processes a GET request to the local server.

    Overridden from SimpleHTTPServer.SimpleHTTPRequestHandler.
    """
    split_url = urlparse.urlsplit(self.path)
    base_path = split_url[2]
    if base_path.startswith('/'):
      base_path = base_path[1:]
    args = urlparse.parse_qs(split_url[3])
    if base_path in self.GET_REQUEST_HANDLERS:
      self.GET_REQUEST_HANDLERS[base_path](self, args)
    else:
      SimpleHTTPServer.SimpleHTTPRequestHandler.do_GET(self)

  def do_POST(self):
    """Processes a POST request to the local server.

    Overridden from SimpleHTTPServer.SimpleHTTPRequestHandler.
    """
    form = urlparse.parse_qs(
        self.rfile.read(int(self.headers.getheader('Content-Length'))))
    path = urlparse.urlparse(self.path)[2]
    if path.startswith('/'):
      path = path[1:]
    if path in self.POST_REQUEST_HANDLERS:
      self.POST_REQUEST_HANDLERS[path](self, form)
    else:
      self.send_response(200)
      self.send_header('Content-Type', 'text/plain')
      self.end_headers()
      self.wfile.write('No handler for POST request "%s".' % path)


class ThreadedHTTPServer(SocketServer.ThreadingMixIn,
                         BaseHTTPServer.HTTPServer):
  def __init__(self, server_address, handler_class):
    BaseHTTPServer.HTTPServer.__init__(self, server_address, handler_class)


class PerfTestServer(object):
  """Local server for use by performance tests."""

  def __init__(self, docroot):
    """Initializes the performance test server.

    Args:
      docroot: The directory from which to serve files.
    """
    # The use of 0 means to start the server on an arbitrary available port.
    self._server = ThreadedHTTPServer(('', 0),
                                      PerfTestServerRequestHandler)
    self._server.docroot = docroot
    self._server_thread = threading.Thread(target=self._server.serve_forever)

  def Run(self):
    """Starts the server thread."""
    self._server_thread.start()

  def ShutDown(self):
    """Shuts down the server."""
    self._server.shutdown()
    self._server_thread.join()

  def GetPort(self):
    """Identifies the port number to which the server is currently bound.

    Returns:
      The numeric port number to which the server is currently bound.
    """
    return self._server.server_address[1]


if __name__ == '__main__':
  pyauto_functional.Main()
