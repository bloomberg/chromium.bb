#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Thread Module to take measurement (CPU, memory) at certain intervals.

This class has a while loop with sleep. On every iteration it takes
measurements. The while loop exits when a member variable (stop_measurement)
is flicked. The parent thread has to set stop_measurement to True, and do
thread.join() to wait for this thread to terminate.
"""
from threading import Thread
import time

from ui_perf_test_utils import UIPerfTestUtils


class UIPerfTestMeasureThread(Thread):
  """A class to take measurements (CPU, memory) at certain intervals."""
  # Instance variables that are used across methods.
  chrome_renderer_process_info = []
  stop_measurement = False
  start_time = 0

  def __init__(self, time_interval=1.0):
    """Init for UIPerfTestMeasureThread.

    Args:
      time_interval: measurement intervals (in second). Please note that
        it is not possible to get accurate interval because of timing issue
        using thread.
    """
    Thread.__init__(self)
    self.time_interval = time_interval
    self.chrome_renderer_process_info = []

  def run(self):
    """Run method that contains loops for measurement."""
    self.start_time = time.time()
    while 1:
      if self.stop_measurement:
        break
      measure_start_time = time.time()
      self._TakeMeasurement()
      measure_elapsed_time = time.time() - measure_start_time
      time_interval = self.time_interval - (measure_elapsed_time / 1000)
      if time_interval > 0:
        time.sleep(time_interval)

  def _TakeMeasurement(self):
      """Take CPU and memory measurement for Chrome renderer process.

      After the measurement, append them to chrome_renderer_process_info
      for presentation later.
      """
      info = UIPerfTestUtils.GetChromeRendererProcessInfo(self.start_time)
      if info is not None:
        self.chrome_renderer_process_info.append(info)


def Main():
  """Test this thread using sample data and Chrome process information.

  You have to start Chrome before you run this.
  """
  chrome_renderer_process_infos = []
  for i in range(1):
    # Pre-processing.
    measure_thread = UIPerfTestMeasureThread()
    measure_thread.start()
    # Emulate process to be measured by sleeping.
    time.sleep(5)
    # Post-processing.
    measure_thread.stop_measurement = True
    measure_thread.join(5)
    chrome_renderer_process_infos.append(
        perf_thread.chrome_renderer_process_info)

  chrome_process_info_names = ['measure-time', 'pct-cpu', 'cpu-user',
                               'cpu-system', 'memory-rss', 'memory-vms',
                               'pct-process-memory']
  chrome_process_info_units = ['sec', 'percent', 'load',
                               'load', 'MB', 'MB', 'percent']
  print UIPerfTestUtils.PrintMeasuredData(
         chrome_renderer_process_infos,
         chrome_process_info_names,
         chrome_process_info_units,
         False, 'p', 'title')


if __name__ == "__main__":
  Main()
