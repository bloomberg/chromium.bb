#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for the contents of battery_utils.py
"""

import logging
import os
import sys
import unittest

from pylib import android_commands
from pylib import constants
from pylib.device import battery_utils
from pylib.device import device_errors
from pylib.device import device_utils
from pylib.device import device_utils_test
from pylib.utils import mock_calls

# RunCommand from third_party/android_testrunner/run_command.py is mocked
# below, so its path needs to be in sys.path.
sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'android_testrunner'))

sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'pymock'))
import mock # pylint: disable=F0401


class BatteryUtilsTest(mock_calls.TestCase):

  def setUp(self):
    self.adb = device_utils_test._AdbWrapperMock('0123456789abcdef')
    self.device = device_utils.DeviceUtils(
        self.adb, default_timeout=10, default_retries=0)
    self.watchMethodCalls(self.call.adb, ignore=['GetDeviceSerial'])
    self.battery = battery_utils.BatteryUtils(
        self.device, default_timeout=10, default_retries=0)


class BatteryUtilsInitTest(unittest.TestCase):

  def testInitWithDeviceUtil(self):
    serial = '0fedcba987654321'
    a = android_commands.AndroidCommands(device=serial)
    d = device_utils.DeviceUtils(a)
    b = battery_utils.BatteryUtils(d)
    self.assertEqual(d, b._device)

  def testInitWithMissing_fails(self):
    with self.assertRaises(TypeError):
      battery_utils.BatteryUtils(None)
    with self.assertRaises(TypeError):
      battery_utils.BatteryUtils('')


class BatteryUtilsSetChargingTest(BatteryUtilsTest):

  @mock.patch('time.sleep', mock.Mock())
  def testSetCharging_enabled(self):
    with self.assertCalls(
        (self.call.device.FileExists(mock.ANY), True),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.device.GetCharging(), False),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.device.GetCharging(), True)):
      self.battery.SetCharging(True)

  def testSetCharging_alreadyEnabled(self):
    with self.assertCalls(
        (self.call.device.FileExists(mock.ANY), True),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.device.GetCharging(), True)):
      self.battery.SetCharging(True)

  @mock.patch('time.sleep', mock.Mock())
  def testSetCharging_disabled(self):
    with self.assertCalls(
        (self.call.device.FileExists(mock.ANY), True),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.device.GetCharging(), True),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.device.GetCharging(), False)):
      self.battery.SetCharging(False)


class BatteryUtilsSetBatteryMeasurementTest(BatteryUtilsTest):

  def testBatteryMeasurement(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            mock.ANY, retries=0, single_line=True,
            timeout=10, check_return=True), '22'),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '--reset'], check_return=True), []),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '--charged', '--checkin'],
            check_return=True), []),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'battery', 'set', 'usb', '0'], check_return=True), []),
        (self.call.device.GetCharging(), False),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'battery', 'set', 'usb', '1'], check_return=True), []),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'battery', 'reset'], check_return=True), []),
        (self.call.device.GetCharging(), True)):
      with self.battery.BatteryMeasurement():
        pass


class BatteryUtilsGetPowerData(BatteryUtilsTest):

  _DUMPSYS_OUTPUT = [
      '9,0,i,uid,1000,test_package1',
      '9,0,i,uid,1001,test_package2',
      '9,1000,l,pwi,uid,1',
      '9,1001,l,pwi,uid,2'
  ]

  def testGetPowerData(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '-c'], check_return=True),
            self._DUMPSYS_OUTPUT)):
      data = self.battery.GetPowerData()
      check = {
          'test_package1': {'uid': '1000', 'data': [1.0]},
          'test_package2': {'uid': '1001', 'data': [2.0]}
      }
      self.assertEqual(data, check)

  def testGetPackagePowerData(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '-c'], check_return=True),
            self._DUMPSYS_OUTPUT)):
      data = self.battery.GetPackagePowerData('test_package2')
      self.assertEqual(data, {'uid': '1001', 'data': [2.0]})


if __name__ == '__main__':
  logging.getLogger().setLevel(logging.DEBUG)
  unittest.main(verbosity=2)
