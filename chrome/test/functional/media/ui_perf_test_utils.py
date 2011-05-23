#!/usr/bin/python

# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for performance testing using the psutil library.

Ref: http://code.google.com/p/psutil/wiki/Documentation

Most part of this module is from chrome/test/startup/startup_test.cc and
chrome/test/ui/ui_perf_test.[h,cc] So, we try to preserve the original C++ code
here in case when there is change in original C++ code, it is easy to update
this.
"""

# Standard library imports.
import re
import time

# Third-party imports.
import psutil


class UIPerfTestUtils:
  """Static utility functions for performance testing."""

  @staticmethod
  def ConvertDataListToString(data_list):
    """Convert data array to string that can be used for results on BuildBot.

    Full accuracy of the results coming from the psutil library is not needed
    for Perf on BuildBot. For now, we show 5 digits here. This function goes
    through the elements in the data_list and does the conversion as well as
    adding a prefix and suffix.

    Args:
      data_list: data list contains measured data from perf test.

    Returns:
      a string that can be used for perf result shown on Buildbot.
    """
    output = '['
    for data in data_list:
      output += ('%.5f' % data) + ', '
    # Remove the last ', '.
    if output.endswith(', '):
      output = output[:-2]
    output += ']'
    return output

  @staticmethod
  def GetResultStringForPerfBot(measurement, modifier, trace, values, units):
    """Get a result string in a format that can be displayed on the PerfBot.

    The following are acceptable (it can be shown in PerfBot) format:
      <*>RESULT <graph_name>: <trace_name>= <value> <units>
      <*>RESULT <graph_name>: <trace_name>= {<mean>, <std deviation>} <units>
      <*>RESULT <graph_name>: <trace_name>= [<value>,value,value,...,] <units>

    Args:
      measurement: measurement string (such as a parameter list).
      modifier: modifier string (such as a file name).
      trace: trace string used for PerfBot graph name (such as 't' or 't_ref').
      values: list of values that displayed as "[value1,value2....]".
      units: units of values such as "sec" or "msec".

    Returns:
      An output string that contains all information, or the empty string if
        there is no information available.
    """
    if not values:
      return ''
    output_string = '%sRESULT %s%s: %s= %s %s' % (
        '', measurement, modifier, trace,
        UIPerfTestUtils.ConvertDataListToString(values), units)
    return output_string

  @staticmethod
  def FindProcesses(process_name):
    """Find processes for a given process name.

    Args:
      process_name: a process name string to find.

    Returns:
      a list of psutil process instances that are associated with the given
        process name.
    """
    target_process_list = []
    for pid in psutil.get_pid_list():
      try:
        p = psutil.Process(pid)
        # Exact match does not work
        if process_name in p.name:
          target_process_list.append(p)
      except psutil.NoSuchProcess:
        # Do nothing since the process is already terminated
        pass
    return target_process_list

  @staticmethod
  def GetResourceInfo(process, start_time):
    """Get resource information coming from psutil.

    This calls corresponding functions in psutil and parses the results.

    TODO(imasaki@chromium.org): Modify this function so that it's not
      hard-coded to return 7 pieces of information.  Instead, you have the
      caller somehow indicate the number and types of information it needs.
      Then the function finds and returns the requested info.

    Args:
      start_time: the time when the program starts (used for recording
        measured_time).
      process: psutil's Process instance.

    Returns:
      a process info tuple: measured_time, cpu_time in percent,
        user cpu time, system cpu time, resident memory size,
        virtual memory size, and memory usage. None is returned if the
        resource info cannot be identified.
    """
    try:
      measured_time = time.time()
      cpu_percent = process.get_cpu_percent(interval=1.0)
      memory_percent = process.get_memory_percent()
      m1 = re.search(r'cputimes\(user=(\S+),\s+system=(\S+)\)',
                     str(process.get_cpu_times()))
      m2 = re.search(r'meminfo\(rss=(\S+),\s+vms=(\S+)\)',
                     str(process.get_memory_info()))

      cputimes_user = float(m1.group(1))
      cputimes_system = float(m1.group(2))

      # Convert Bytes to MBytes.
      memory_rss = float(m2.group(1)) / 1000000
      memory_vms = float(m2.group(2)) / 1000000

      return (measured_time - start_time, cpu_percent, cputimes_user,
              cputimes_system, memory_rss, memory_vms, memory_percent)

    except psutil.NoSuchProcess:
      # Do nothing since the process is already terminated.
      # This may happen due to race condition.
      return None

  @staticmethod
  def IsChromeRendererProcess(process):
    """Check whether the given process is a Chrome Renderer process.

    Args:
      process: a psutil's Process instance.

    Returns:
      True if process is a Chrome renderer process. False otherwise.
    """
    for line in process.cmdline:
      if 'type=renderer' in line:
        return True
    return False

  @staticmethod
  def GetChromeRendererProcessInfo(start_time):
    """Get Chrome renderer process information by psutil.

    Returns:
      a renderer process info tuple: measured_time, cpu_time in
        percent, user cpu time, system cpu time, resident memory size, virtual
        memory size, and memory usage. Or returns an empty list if the Chrome
        renderer process is not found.
    """
    chrome_process_list = UIPerfTestUtils.FindProcesses('chrome')
    for p in chrome_process_list:
      if UIPerfTestUtils.IsChromeRendererProcess(p):
        # Return the first renderer process's resource info.
        resource_info = UIPerfTestUtils.GetResourceInfo(p, start_time)
        if resource_info is not None:
          return resource_info
    return []

  @staticmethod
  def __getMaxDataLength(chrome_renderer_process_infos):
    """Get max data length of process render info.

    This method is necessary since reach run may have different data length.
    So, you have to get maximum to prevent data from missing.

    Args:
      measured_data_list : measured_data_list that
      contain a list of measured data (CPU and memory) at certain intervals
      over several runs. Each run contains several time data.
        info -> 0th run -> 0th time -> time stamp, CPU data and memory data
                        -> 1th time -> time stamp, CPU data and memory data
                        .....
             -> 1th run -> 0th time -> time stamp, CPU data and memory data
      each run may have different number of measurement.

    Returns:
      max data length among all runs.
    """
    maximum = len(chrome_renderer_process_infos[0])
    for info in chrome_renderer_process_infos:
      if maximum < len(info):
        maximum = len(info)
    return maximum

  @staticmethod
  def PrintMeasuredData(measured_data_list, measured_data_name_list,
                        measured_data_unit_list, remove_first_result,
                        parameter_string, title):
    """Calculate statistics over all results and print them in the format that
    can be shown on BuildBot.

    Args:
      measured_data_list: measured_data_list that contains a list of measured
        data at certain intervals over several runs. Each run should contain
        the timestamp of the measured time as well.
          info -> 0th run -> 0th time -> list of measured data
                                         (defined in measured_data_name_list)
                          -> 1st time -> list of measured data
                          .....
               -> 1st run -> 0th time -> list of measured data
        each run may have different number of measurement.
      measured_data_name_list: a list of the names for an element of
        measured_data_list (such as 'measured-time','cpu'). The size of this
        list should be same as the size of measured_data_unit_list.
      measured_data_unit_list: a list of the names of the units for an element
        of measured_data_list. The size of this list should be same as the size
        of measured_data_name_list.
      remove_first_result: a boolean for removing the first result
         (the first result contains browser startup time).
      parameter_string: a string that contains all parameters used.
      title: a title string for identifying perf data.
    Returns:
      An output string that contains all information, or the empty string
        if there is no information to output.
    """
    output_string = ''
    for i in range(len(measured_data_name_list)):
      max_data_length = UIPerfTestUtils.__getMaxDataLength(
          measured_data_list)
      for time_index in range(max_data_length):
        psutil_data = []
        for counter in range(len(measured_data_list)):
          if not remove_first_result or counter > 0:
            data_length_for_each = (
                len(measured_data_list[counter]))
            if (data_length_for_each > time_index):
              data = measured_data_list[counter][time_index][i]
              psutil_data.append(data)
        name = measured_data_name_list[i] + '-' + str(time_index)
        output_string_line = UIPerfTestUtils.GetResultStringForPerfBot(
            parameter_string + '-', name, title, psutil_data,
            measured_data_unit_list[i])
        if output_string_line:
          output_string += output_string_line + '\n'
    return output_string
