# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for the contents of device_utils.py (mostly DeviceUtils).
"""

# pylint: disable=W0212
# pylint: disable=W0613

import random
import time
import unittest

from pylib import android_commands
from pylib import cmd_helper
from pylib.device import adb_wrapper
from pylib.device import device_errors
from pylib.device import device_utils


class DeviceUtilsTest(unittest.TestCase):
  def testGetAVDs(self):
    pass

  def testRestartServerNotRunning(self):
    self.assertEqual(0, cmd_helper.RunCmd(['pkill', 'adb']),
                     msg='Unable to kill adb during setup.')
    self.assertNotEqual(0, cmd_helper.RunCmd(['pgrep', 'adb']),
                        msg='Unexpectedly found adb during setup.')
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

  @staticmethod
  def _getTestAdbWrapper():
    devices = adb_wrapper.AdbWrapper.GetDevices()
    start_time = time.time()
    while time.time() - start_time < device_utils._DEFAULT_TIMEOUT:
      if devices:
        return random.choice(devices)
      time.sleep(1)
    raise device_errors.DeviceUnreachableError(
        'No devices available for testing...')

  @staticmethod
  def _getUnusedSerial():
    used_devices = [str(d) for d in adb_wrapper.AdbWrapper.GetDevices()]
    while True:
      serial = ''
      for _ in xrange(0, 16):
        serial += random.choice('0123456789abcdef')
      if serial not in used_devices:
        return serial

  def testIsOnline(self):
    d = device_utils.DeviceUtils(self._getTestAdbWrapper())
    self.assertTrue(d is None or d.IsOnline())
    d = device_utils.DeviceUtils(self._getUnusedSerial())
    self.assertFalse(d.IsOnline())

  def testHasRoot(self):
    a = self._getTestAdbWrapper()
    d = device_utils.DeviceUtils(a)
    secure_prop = a.Shell('getprop ro.secure').strip()
    if secure_prop == '1':
      build_type_prop = a.Shell('getprop ro.build.type').strip()
      if build_type_prop == 'userdebug':
        adb_root_prop = a.Shell('getprop service.adb.root').strip()
        if adb_root_prop is None or adb_root_prop == '0':
          self.assertFalse(d.HasRoot())
        else:
          self.assertTrue(d.HasRoot())
      else:
        self.assertFalse(d.HasRoot())
    else:
      self.assertTrue(d.HasRoot())

  def testEnableRoot(self):
    a = self._getTestAdbWrapper()
    d = device_utils.DeviceUtils(a)

    secure_prop = a.Shell('getprop ro.secure').strip()
    if secure_prop == '1':
      build_type_prop = a.Shell('getprop ro.build.type').strip()
      if build_type_prop == 'userdebug':
        # Turn off the adb root property
        adb_root_prop = a.Shell('getprop service.adb.root').strip()
        if adb_root_prop == '1':
          a.Shell('setprop service.adb.root 0')

        # Make sure that adbd is running without root by restarting it
        ps_out = a.Shell('ps')
        adbd_pids = []
        for line in ps_out.splitlines():
          if 'adbd' in line:
            pid = line.split()[1]
            adbd_pids.append(pid)
        for pid in adbd_pids:
          a.Shell('kill %s' % str(pid))
        a.WaitForDevice()

        self.assertFalse(d.HasRoot())
        d.EnableRoot()
        self.assertTrue(d.HasRoot())
      else:
        self.assertFalse(d.HasRoot())
        with self.assertRaises(device_errors.CommandFailedError):
          d.EnableRoot()
    else:
      self.assertTrue(d.HasRoot())
      d.EnableRoot()
      self.assertTrue(d.HasRoot())

  def testGetExternalStorage(self):
    a = self._getTestAdbWrapper()
    d = device_utils.DeviceUtils(a)

    actual_external_storage = a.Shell('echo $EXTERNAL_STORAGE').splitlines()[0]
    if actual_external_storage and len(actual_external_storage) != 0:
      self.assertEquals(actual_external_storage, d.GetExternalStoragePath())

  def testWaitUntilFullyBooted(self):
    a = self._getTestAdbWrapper()
    d = device_utils.DeviceUtils(a)

    a.Reboot()
    while a.GetState() == 'device':
      time.sleep(0.1)
    d.WaitUntilFullyBooted(wifi=True)
    self.assertEquals(
        '1', a.Shell('getprop sys.boot_completed').splitlines()[0])
    self.assertTrue(
        a.Shell('pm path android').splitlines()[0].startswith('package:'))
    self.assertTrue(a.Shell('ls $EXTERNAL_STORAGE'))
    self.assertTrue(
        'Wi-Fi is enabled' in a.Shell('dumpsys wifi').splitlines())


if __name__ == '__main__':
  unittest.main(verbosity=2, buffer=True)

