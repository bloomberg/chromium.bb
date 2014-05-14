# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for the contents of device_utils.py (mostly DeviceUtils).
"""

# pylint: disable=W0212
# pylint: disable=W0613

import unittest
from pylib import android_commands
from pylib import cmd_helper
from pylib.device import adb_wrapper
from pylib.device import device_utils


class DeviceUtilsTest(unittest.TestCase):
  def testGetAVDs(self):
    pass

  def testRestartServerNotRunning(self):
    # TODO(jbudorick) If these fail, it's not DeviceUtils's fault.
    self.assertEqual(0, cmd_helper.RunCmd(['pkill', 'adb']))
    self.assertNotEqual(0, cmd_helper.RunCmd(['pgrep', 'adb']))
    device_utils.RestartServer()
    self.assertEqual(0, cmd_helper.RunCmd(['pgrep', 'adb']))

  def testRestartServerAlreadyRunning(self):
    if cmd_helper.RunCmd(['pgrep', 'adb']) != 0:
      device_utils.RestartServer()
    code, original_pid = cmd_helper.GetCmdStatusAndOutput(['pgrep', 'adb'])
    self.assertEqual(0, code)
    device_utils.RestartServer()
    code, new_pid = cmd_helper.GetCmdStatusAndOutput(['pgrep', 'adb'])
    self.assertEqual(0, code)
    self.assertNotEqual(original_pid, new_pid)

  def testInitWithStr(self):
    serial_as_str = str('0123456789abcdef')
    d = device_utils.DeviceUtils('0123456789abcdef')
    self.assertEqual(serial_as_str, d.old_interface.GetDevice())

  def testInitWithUnicode(self):
    serial_as_unicode = unicode('fedcba9876543210')
    d = device_utils.DeviceUtils(serial_as_unicode)
    self.assertEqual(serial_as_unicode, d.old_interface.GetDevice())

  def testInitWithAdbWrapper(self):
    serial = '123456789abcdef0'
    a = adb_wrapper.AdbWrapper(serial)
    d = device_utils.DeviceUtils(a)
    self.assertEqual(serial, d.old_interface.GetDevice())

  def testInitWithAndroidCommands(self):
    serial = '0fedcba987654321'
    a = android_commands.AndroidCommands(device=serial)
    d = device_utils.DeviceUtils(a)
    self.assertEqual(serial, d.old_interface.GetDevice())

  def testInitWithNone(self):
    d = device_utils.DeviceUtils(None)
    self.assertIsNone(d.old_interface.GetDevice())


if __name__ == '__main__':
  unittest.main()

