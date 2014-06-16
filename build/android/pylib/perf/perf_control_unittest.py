# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

sys.path.append(os.path.join(os.path.dirname(__file__), os.pardir, os.pardir))

from pylib import android_commands
from pylib.device import device_utils
from pylib.perf import perf_control


class TestPerfControl(unittest.TestCase):
  def setUp(self):
    if not os.getenv('BUILDTYPE'):
      os.environ['BUILDTYPE'] = 'Debug'

    devices = android_commands.GetAttachedDevices()
    self.assertGreater(len(devices), 0, 'No device attached!')
    self._device = device_utils.DeviceUtils(
        android_commands.AndroidCommands(device=devices[0]))

  def testForceAllCpusOnline(self):
    perf = perf_control.PerfControl(self._device)
    cpu_online_files = self._device.RunShellCommand(
        'ls -d /sys/devices/system/cpu/cpu[0-9]*/online')
    try:
      perf.ForceAllCpusOnline(True)
      for path in cpu_online_files:
        self.assertEquals('1',
                          self._device.old_interface.GetFileContents(path)[0])
        mode = self._device.RunShellCommand('ls -l %s' % path)[0]
        self.assertEquals('-r--r--r--', mode[:10])
    finally:
      perf.ForceAllCpusOnline(False)

    for path in cpu_online_files:
      mode = self._device.RunShellCommand('ls -l %s' % path)[0]
      self.assertEquals('-rw-r--r--', mode[:10])


if __name__ == '__main__':
  unittest.main()
