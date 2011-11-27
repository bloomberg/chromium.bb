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
                     msg='result output is wrong')

  def testGetResultStringForPerfBot(self):
    """Test PrintResultList method."""
    times = [1.023, 2.323, 2.44232]
    output_string = UIPerfTestUtils.GetResultStringForPerfBot(
        'playback', '', 'bear', times, 'ms')
    self.assertEqual(output_string,
                     'RESULT playback: bear= [1.02300, 2.32300, 2.44232] ms',
                     msg='result output is wrong')

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
    self.assertEqual(len(info), 7, msg='the length of info should be 7')
    for i in range(len(info)):
     self.assertTrue(info[i] is not None, msg='process info has None data')

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

  def _PrintMeasuredDataTestHelper(self, show_time_index,
                                   expected_output_string,
                                   display_filter=None):
    """A helper function for tests testPrintMeasuredData*.

    Create fake process and call PrintMeasuredData with appropriate arguments.

    Args:
      show_time_index: call PrintMeasuredData with this show_time_index.
      expected_output_string: the expected result string to be compared.
      display_filter: run test with this display_filter, which specifies which
        measurements to display.
    """
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
    chrome_process_trace_list = ['t', 'p', 'l', 'l', 'm', 'm', 'p']

    output_string = UIPerfTestUtils.PrintMeasuredData(
        measured_data_list=chrome_renderer_process_infos,
        measured_data_name_list=chrome_process_info_names,
        measured_data_unit_list=chrome_process_info_units,
        parameter_string='', trace_list=chrome_process_trace_list,
        show_time_index=show_time_index, remove_first_result=False,
        display_filter=display_filter)

    self.assertEqual(output_string, expected_output_string,
                     msg=('output string is wrong'
                          '\nexpected:\n%s \nactual:\n%s') % (
                              expected_output_string, output_string))

  def testPrintMeasuredDataShowTimeIndex(self):
     expected_output_string = (
         'RESULT measure-time-0: t= [10.00000, 10.00000] sec\n'
         'RESULT measure-time-1: t= [20.00000] sec\n'
         'RESULT pct-cpu-0: p= [11.00000, 11.00000] percent\n'
         'RESULT pct-cpu-1: p= [21.00000] percent\n'
         'RESULT cpu-user-0: l= [12.00000, 12.00000] load\n'
         'RESULT cpu-user-1: l= [22.00000] load\n'
         'RESULT cpu-system-0: l= [13.00000, 13.00000] load\n'
         'RESULT cpu-system-1: l= [23.00000] load\n'
         'RESULT memory-rss-0: m= [14.00000, 14.00000] MB\n'
         'RESULT memory-rss-1: m= [24.00000] MB\n'
         'RESULT memory-vms-0: m= [15.00000, 15.00000] MB\n'
         'RESULT memory-vms-1: m= [25.00000] MB\n'
         'RESULT pct-process-memory-0: p= [16.00000, 16.00000] percent\n'
         'RESULT pct-process-memory-1: p= [26.00000] percent\n')
     self._PrintMeasuredDataTestHelper(True, expected_output_string)

  def testPrintMeasuredDataNoShowTimeIndex(self):
    expected_output_string = (
        'RESULT measure-time: t= [10.00000, 10.00000, 20.00000] sec\n'
        'RESULT pct-cpu: p= [11.00000, 11.00000, 21.00000] percent\n'
        'RESULT cpu-user: l= [12.00000, 12.00000, 22.00000] load\n'
        'RESULT cpu-system: l= [13.00000, 13.00000, 23.00000] load\n'
        'RESULT memory-rss: m= [14.00000, 14.00000, 24.00000] MB\n'
        'RESULT memory-vms: m= [15.00000, 15.00000, 25.00000] MB\n'
        'RESULT pct-process-memory: p= [16.00000, 16.00000, 26.00000]'
        ' percent\n')
    self._PrintMeasuredDataTestHelper(False, expected_output_string)

  def testPrintMeasuredDataNoShowTimeIndexWithDisplayFilter(self):
    expected_output_string = (
        'RESULT pct-cpu: p= [11.00000, 11.00000, 21.00000] percent\n')
    self._PrintMeasuredDataTestHelper(False, expected_output_string,
                                      display_filter=['pct-cpu'])
