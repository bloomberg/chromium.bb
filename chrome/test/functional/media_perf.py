#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Performance test for HTML5 media tag.

This PyAuto powered script plays media (video or audio) files using the HTML5
tag embedded in an HTML file (specified in the GetPlayerHTMLFileName() method)
and measures CPU and memory usage using the psutil library in a different
thread using the UIPerfTestMeasureThread class. The parameters needed to
run this test are passed in the form of environment variables
(such as the number of runs). media_perf_runner.py is used for
generating these variables (PyAuto does not support direct parameters).

Ref: http://code.google.com/p/psutil/wiki/Documentation
"""

import os
import time

import pyauto_functional  # Must be imported before pyauto.
import pyauto

from media_test_base import MediaTestBase
from media_test_env_names import MediaTestEnvNames
from ui_perf_test_measure_thread import UIPerfTestMeasureThread
from ui_perf_test_utils import UIPerfTestUtils


class MediaPerformanceTest(MediaTestBase):
  """Tests for basic media performance."""
  # Since PyAuto does not support commandline argument, we have to rely on
  # environment variables. The followings are the names of the environment
  # variables that are used in the tests.
  # Time interval between measurement.
  DEFAULT_MEASURE_INTERVAL = 1
  TIMEOUT = 10000

  # These predefined names are coming from library psutil
  # except for 'measure-time' which represents the timestamp at the start
  # of program execution.
  CHROME_PROCESS_INFO_NAMES = ['measure-time',
                               'pct-cpu',
  # pct-cpu: a float representing the current system-wide CPU utilization
  # as a percentage. When interval is > 0.0 compares system CPU times
  # elapsed before and after the interval (blocking).
                               'cpu-user',
                               'cpu-system',
  # cpu-user, cpu-system: process CPU user and system times which means
  # the amount of time expressed in seconds that a process has spent in
  # user/system mode.
                               'memory-rss',
                               'memory-vms',
  # memory-rss, memory-vms: values representing RSS (Resident Set Size) and
  # VMS (Virtual Memory Size) in bytes.
                               'pct-process-memory']
  # pct-process-memory: compare physical system memory to process resident
  # memory and calculate process memory utilization as a percentage.
  CHROME_PROCESS_INFO_UNITS = ['sec',
                               'percent',
                               'load',
                               'load',
                               'MB',
                               'MB',
                               'percent']
  # Instance variables.
  run_counter = 0
  chrome_renderer_process_infos = []
  measure_thread = None

  def testHTML5MediaTag(self):
    """Test the HTML5 media tag."""
    MediaTestBase.ExecuteTest(self)

  def PreAllRunsProcess(self):
    """A method to execute before all runs."""
    MediaTestBase.PreAllRunsProcess(self)
    self.chrome_renderer_process_infos = []
    for i in range(self.number_of_runs):
      self.chrome_renderer_process_infos.append([])

  def PostAllRunsProcess(self):
    """A method to execute after all runs."""
    MediaTestBase.PostAllRunsProcess(self)
    print UIPerfTestUtils.PrintMeasuredData(
        measured_data_list=self.chrome_renderer_process_infos,
        measured_data_name_list=self.CHROME_PROCESS_INFO_NAMES,
        measured_data_unit_list=self.CHROME_PROCESS_INFO_UNITS,
        remove_first_result=self.remove_first_result,
        parameter_string=self.parameter_str,
        title=self.media_filename_nickname)

  def PreEachRunProcess(self, run_counter):
    """A method to execute before each run.

    Starts a thread that measures the performance.

    Args:
      run_counter: a counter for each run.
    """
    MediaTestBase.PreEachRunProcess(self, run_counter)

    self.run_counter = run_counter
    measure_intervals = os.getenv(MediaTestEnvNames.MEASURE_INTERVAL_ENV_NAME,
                                  self.DEFAULT_MEASURE_INTERVAL)
    # Start the thread.
    self.measure_thread = UIPerfTestMeasureThread()
    self.measure_thread.start()

  def PostEachRunProcess(self, run_counter):
    """A method to execute after each run.

    Terminates the measuring thread and records the measurement in
    measure_thread.chrome_renderer_process_info.

    Args:
      run_counter: a counter for each run.
    """
    MediaTestBase.PostEachRunProcess(self, run_counter)
    # Record the measurement data.
    self.chrome_renderer_process_infos[run_counter] = (
        self.measure_thread.chrome_renderer_process_info)
    # Join the thread.
    self.measure_thread.stop_measurement = True
    self.measure_thread.join(self.TIMEOUT)


if __name__ == '__main__':
  pyauto_functional.Main()
