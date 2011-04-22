#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
import unittest

from ui_perf_test_utils import UIPerfTestUtils


class TestUIPerfUtils(unittest.TestCase):
  """Test UIPerfUtils class."""

  def testConvertDataListToString(self):
    times = [1.023344324, 2.3233333, 2.442324444]
    output_string = UIPerfTestUtils.ConvertDataListToString(times)
    self.assertEqual(output_string, '[1.02334, 2.32333, 2.44232]',
                     'result output is wrong')

  def testGetResultStringForPerfBot(self):
    """Test PrintResultList method."""
    times = [1.023, 2.323, 2.44232]
    output_string = UIPerfTestUtils.GetResultStringForPerfBot(
        'playback', '', 'bear', times, 'ms')
    self.assertEqual(output_string,
                     'RESULT playback: bear= [1.02300, 2.32300, 2.44232] ms',
                     'result output is wrong')

  def testGetResultStringForPerfBotEmptyData(self):
    """Test PrintResultList method with empty data."""
    times = []
    output_string = UIPerfTestUtils.GetResultStringForPerfBot(
        'playback', '', 'bear', times, 'ms')
    self.assertFalse(output_string, msg='Result output is not empty.')

  def testFindProcessesAndGetResourceInfo(self):
    """Test FindProcesses and GetResourceInfo methods.

    Python process should be found when we run this script. Assert all
    elements in processInfo are not None.
    """
    list = UIPerfTestUtils.FindProcesses('python')
    self.assertTrue(len(list) > 0, 'python process cannot be found')
    info = UIPerfTestUtils.GetResourceInfo(list[0], time.time())
    self._AssertProcessInfo(info)

  def GetChromeRendererProcessInfo(self):
    """Test GetChromeRendererProcessInfo method.

    You must start Chrome before you run your test. Otherwise, it fails.
    So, this test is not included in the unit test (i.e., the method name
    does not start with "test").

    TODO(imasaki@chromium.org): find a way to start Chrome automatically.
    """
    start_time = time.time()
    info = UIPerfTestUtils.GetChromeRendererProcessInfo(start_time)
    self._AssertProcessInfo(info)

  def _AssertProcessInfo(self, info):
    """Assert process info has correct length and each element is not null."""
    # See UIPerfTestUtils.chrome_process_info_names.
    self.assertEqual(len(info), 7, 'the length of info should be 7')
    for i in range(len(info)):
     self.assertTrue(info[i] is not None, 'process info has None data')

  def _CreateFakeProcessInfo(self, time, process_info_length):
    """Create fake process info for testing.

    Args:
      time: time used for measured_time.

    Returns:
      a process info with some data for testing.
    """
    chrome_renderer_process_info = []
    for i in range(process_info_length):
      chrome_renderer_process_info.append(i + time)
    return chrome_renderer_process_info

  def testPrintMeasuredData(self):
     # Build process info for testing.
     chrome_renderer_process_infos = []
     run_info1 = []
     run_info1.append(self._CreateFakeProcessInfo(10, 7))
     run_info1.append(self._CreateFakeProcessInfo(20, 7))
     chrome_renderer_process_infos.append(run_info1)
     run_info2 = []
     run_info2.append(self._CreateFakeProcessInfo(10, 7))
     chrome_renderer_process_infos.append(run_info2)
     chrome_process_info_names = ['measure-time', 'pct-cpu', 'cpu-user',
                                  'cpu-system', 'memory-rss', 'memory-vms',
                                  'pct-process-memory']
     chrome_process_info_units = ['sec', 'percent', 'load',
                                  'load', 'MB', 'MB', 'percent']
     output_string = UIPerfTestUtils.PrintMeasuredData(
         chrome_renderer_process_infos,
         chrome_process_info_names,
         chrome_process_info_units,
         False, 'p', 'title')
     expected_output_string = (
         'RESULT p-measure-time-0: title= [10.00000, 10.00000] sec\n'
         'RESULT p-measure-time-1: title= [20.00000] sec\n'
         'RESULT p-pct-cpu-0: title= [11.00000, 11.00000] percent\n'
         'RESULT p-pct-cpu-1: title= [21.00000] percent\n'
         'RESULT p-cpu-user-0: title= [12.00000, 12.00000] load\n'
         'RESULT p-cpu-user-1: title= [22.00000] load\n'
         'RESULT p-cpu-system-0: title= [13.00000, 13.00000] load\n'
         'RESULT p-cpu-system-1: title= [23.00000] load\n'
         'RESULT p-memory-rss-0: title= [14.00000, 14.00000] MB\n'
         'RESULT p-memory-rss-1: title= [24.00000] MB\n'
         'RESULT p-memory-vms-0: title= [15.00000, 15.00000] MB\n'
         'RESULT p-memory-vms-1: title= [25.00000] MB\n'
         'RESULT p-pct-process-memory-0: title= [16.00000, 16.00000] percent\n'
         'RESULT p-pct-process-memory-1: title= [26.00000] percent\n')
     self.assertEqual(output_string, expected_output_string,
                      'output string is wrong')
