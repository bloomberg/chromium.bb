# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import time

from chrome_profiler import controllers

from pylib import pexpect


class ChromeTracingController(controllers.BaseController):
  def __init__(self, device, package_info, categories, ring_buffer):
    controllers.BaseController.__init__(self)
    self._device = device
    self._package_info = package_info
    self._categories = categories
    self._ring_buffer = ring_buffer
    self._trace_file = None
    self._trace_interval = None
    self._trace_start_re = \
       re.compile(r'Logging performance trace to file')
    self._trace_finish_re = \
       re.compile(r'Profiler finished[.] Results are in (.*)[.]')
    self._device.old_interface.StartMonitoringLogcat(clear=False)

  def __repr__(self):
    return 'chrome trace'

  @staticmethod
  def GetCategories(device, package_info):
    device.old_interface.BroadcastIntent(
        package_info.package, 'GPU_PROFILER_LIST_CATEGORIES')
    try:
      json_category_list = device.old_interface.WaitForLogMatch(
          re.compile(r'{"traceCategoriesList(.*)'), None, timeout=5).group(0)
    except pexpect.TIMEOUT:
      raise RuntimeError('Performance trace category list marker not found. '
                         'Is the correct version of the browser running?')

    record_categories = []
    disabled_by_default_categories = []
    json_data = json.loads(json_category_list)['traceCategoriesList']
    for item in json_data:
      if item.startswith('disabled-by-default'):
        disabled_by_default_categories.append(item)
      else:
        record_categories.append(item)

    return record_categories, disabled_by_default_categories

  def StartTracing(self, interval):
    self._trace_interval = interval
    self._device.old_interface.SyncLogCat()
    self._device.old_interface.BroadcastIntent(
        self._package_info.package, 'GPU_PROFILER_START',
        '-e categories "%s"' % ','.join(self._categories),
        '-e continuous' if self._ring_buffer else '')
    # Chrome logs two different messages related to tracing:
    #
    # 1. "Logging performance trace to file"
    # 2. "Profiler finished. Results are in [...]"
    #
    # The first one is printed when tracing starts and the second one indicates
    # that the trace file is ready to be pulled.
    try:
      self._device.old_interface.WaitForLogMatch(
          self._trace_start_re, None, timeout=5)
    except pexpect.TIMEOUT:
      raise RuntimeError('Trace start marker not found. Is the correct version '
                         'of the browser running?')

  def StopTracing(self):
    self._device.old_interface.BroadcastIntent(
        self._package_info.package,
        'GPU_PROFILER_STOP')
    self._trace_file = self._device.old_interface.WaitForLogMatch(
        self._trace_finish_re, None, timeout=120).group(1)

  def PullTrace(self):
    # Wait a bit for the browser to finish writing the trace file.
    time.sleep(self._trace_interval / 4 + 1)

    trace_file = self._trace_file.replace('/storage/emulated/0/', '/sdcard/')
    host_file = os.path.join(os.path.curdir, os.path.basename(trace_file))
    self._device.old_interface.PullFileFromDevice(trace_file, host_file)
    return host_file
