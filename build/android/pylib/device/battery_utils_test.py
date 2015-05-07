#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for the contents of battery_utils.py
"""

# pylint: disable=W0613

import logging
import os
import sys
import unittest

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

_DUMPSYS_OUTPUT = [
    '9,0,i,uid,1000,test_package1',
    '9,0,i,uid,1001,test_package2',
    '9,1000,l,pwi,uid,1',
    '9,1001,l,pwi,uid,2'
]


class BatteryUtilsTest(mock_calls.TestCase):

  def ShellError(self, output=None, status=1):
    def action(cmd, *args, **kwargs):
      raise device_errors.AdbShellCommandFailedError(
          cmd, output, status, str(self.device))
    if output is None:
      output = 'Permission denied\n'
    return action

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
    d = device_utils.DeviceUtils(serial)
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
        (self.call.battery.GetCharging(), False),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.battery.GetCharging(), True)):
      self.battery.SetCharging(True)

  def testSetCharging_alreadyEnabled(self):
    with self.assertCalls(
        (self.call.device.FileExists(mock.ANY), True),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.battery.GetCharging(), True)):
      self.battery.SetCharging(True)

  @mock.patch('time.sleep', mock.Mock())
  def testSetCharging_disabled(self):
    with self.assertCalls(
        (self.call.device.FileExists(mock.ANY), True),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.battery.GetCharging(), True),
        (self.call.device.RunShellCommand(mock.ANY, check_return=True), []),
        (self.call.battery.GetCharging(), False)):
      self.battery.SetCharging(False)


class BatteryUtilsSetBatteryMeasurementTest(BatteryUtilsTest):

  def testBatteryMeasurement(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            mock.ANY, retries=0, single_line=True,
            timeout=10, check_return=True), '22'),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'battery', 'reset'], check_return=True), []),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '--reset'], check_return=True), []),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '--charged', '--checkin'],
            check_return=True), []),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'battery', 'set', 'ac', '0'], check_return=True), []),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'battery', 'set', 'usb', '0'], check_return=True), []),
        (self.call.battery.GetCharging(), False),
        (self.call.device.RunShellCommand(
            ['dumpsys', 'battery', 'reset'], check_return=True), []),
        (self.call.battery.GetCharging(), True)):
      with self.battery.BatteryMeasurement():
        pass


class BatteryUtilsGetPowerData(BatteryUtilsTest):

  def testGetPowerData(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '-c'], check_return=True),
            _DUMPSYS_OUTPUT)):
      data = self.battery.GetPowerData()
      check = {
          'test_package1': {'uid': '1000', 'data': [1.0]},
          'test_package2': {'uid': '1001', 'data': [2.0]}
      }
      self.assertEqual(data, check)

  def testGetPowerData_packageCollisionSame(self):
      self.battery._cache['uids'] = {'test_package1': '1000'}
      with self.assertCall(
        self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '-c'], check_return=True),
            _DUMPSYS_OUTPUT):
        data = self.battery.GetPowerData()
        check = {
            'test_package1': {'uid': '1000', 'data': [1.0]},
            'test_package2': {'uid': '1001', 'data': [2.0]}
        }
        self.assertEqual(data, check)

  def testGetPowerData_packageCollisionDifferent(self):
      self.battery._cache['uids'] = {'test_package1': '1'}
      with self.assertCall(
        self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '-c'], check_return=True),
            _DUMPSYS_OUTPUT):
        with self.assertRaises(device_errors.CommandFailedError):
          self.battery.GetPowerData()

  def testGetPowerData_cacheCleared(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '-c'], check_return=True),
            _DUMPSYS_OUTPUT)):
      self.battery._cache.clear()
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
            _DUMPSYS_OUTPUT)):
      data = self.battery.GetPackagePowerData('test_package2')
      self.assertEqual(data, {'uid': '1001', 'data': [2.0]})

  def testGetPackagePowerData_badPackage(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '-c'], check_return=True),
            _DUMPSYS_OUTPUT)):
      data = self.battery.GetPackagePowerData('not_a_package')
      self.assertEqual(data, None)


class BatteryUtilsChargeDevice(BatteryUtilsTest):

  @mock.patch('time.sleep', mock.Mock())
  def testChargeDeviceToLevel(self):
    with self.assertCalls(
        (self.call.battery.SetCharging(True)),
        (self.call.battery.GetBatteryInfo(), {'level': '50'}),
        (self.call.battery.GetBatteryInfo(), {'level': '100'})):
      self.battery.ChargeDeviceToLevel(95)


class BatteryUtilsGetBatteryInfoTest(BatteryUtilsTest):

  def testGetBatteryInfo_normal(self):
    with self.assertCall(
        self.call.device.RunShellCommand(
            ['dumpsys', 'battery'], check_return=True),
        [
          'Current Battery Service state:',
          '  AC powered: false',
          '  USB powered: true',
          '  level: 100',
          '  temperature: 321',
        ]):
      self.assertEquals(
          {
            'AC powered': 'false',
            'USB powered': 'true',
            'level': '100',
            'temperature': '321',
          },
          self.battery.GetBatteryInfo())

  def testGetBatteryInfo_nothing(self):
    with self.assertCall(
        self.call.device.RunShellCommand(
            ['dumpsys', 'battery'], check_return=True), []):
      self.assertEquals({}, self.battery.GetBatteryInfo())


class BatteryUtilsGetChargingTest(BatteryUtilsTest):

  def testGetCharging_usb(self):
    with self.assertCall(
        self.call.battery.GetBatteryInfo(), {'USB powered': 'true'}):
      self.assertTrue(self.battery.GetCharging())

  def testGetCharging_usbFalse(self):
    with self.assertCall(
        self.call.battery.GetBatteryInfo(), {'USB powered': 'false'}):
      self.assertFalse(self.battery.GetCharging())

  def testGetCharging_ac(self):
    with self.assertCall(
        self.call.battery.GetBatteryInfo(), {'AC powered': 'true'}):
      self.assertTrue(self.battery.GetCharging())

  def testGetCharging_wireless(self):
    with self.assertCall(
        self.call.battery.GetBatteryInfo(), {'Wireless powered': 'true'}):
      self.assertTrue(self.battery.GetCharging())

  def testGetCharging_unknown(self):
    with self.assertCall(
        self.call.battery.GetBatteryInfo(), {'level': '42'}):
      self.assertFalse(self.battery.GetCharging())


class BatteryUtilsGetNetworkDataTest(BatteryUtilsTest):

  def testGetNetworkData_noDataUsage(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '-c'], check_return=True),
            _DUMPSYS_OUTPUT),
        (self.call.device.ReadFile('/proc/uid_stat/1000/tcp_snd'),
            self.ShellError()),
        (self.call.device.ReadFile('/proc/uid_stat/1000/tcp_rcv'),
            self.ShellError())):
      self.assertEquals(self.battery.GetNetworkData('test_package1'), (0, 0))

  def testGetNetworkData_badPackage(self):
    with self.assertCall(
        self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '-c'], check_return=True),
            _DUMPSYS_OUTPUT):
      self.assertEqual(self.battery.GetNetworkData('asdf'), None)

  def testGetNetworkData_packageNotCached(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '-c'], check_return=True),
            _DUMPSYS_OUTPUT),
        (self.call.device.ReadFile('/proc/uid_stat/1000/tcp_snd'), 1),
        (self.call.device.ReadFile('/proc/uid_stat/1000/tcp_rcv'), 2)):
      self.assertEqual(self.battery.GetNetworkData('test_package1'), (1,2))

  def testGetNetworkData_packageCached(self):
    self.battery._cache['uids'] = {'test_package1': '1000'}
    with self.assertCalls(
        (self.call.device.ReadFile('/proc/uid_stat/1000/tcp_snd'), 1),
        (self.call.device.ReadFile('/proc/uid_stat/1000/tcp_rcv'), 2)):
      self.assertEqual(self.battery.GetNetworkData('test_package1'), (1,2))

  def testGetNetworkData_clearedCache(self):
    with self.assertCalls(
        (self.call.device.RunShellCommand(
            ['dumpsys', 'batterystats', '-c'], check_return=True),
            _DUMPSYS_OUTPUT),
        (self.call.device.ReadFile('/proc/uid_stat/1000/tcp_snd'), 1),
        (self.call.device.ReadFile('/proc/uid_stat/1000/tcp_rcv'), 2)):
      self.battery._cache.clear()
      self.assertEqual(self.battery.GetNetworkData('test_package1'), (1,2))

class BatteryUtilsLetBatteryCoolToTemperatureTest(BatteryUtilsTest):

  @mock.patch('time.sleep', mock.Mock())
  def testLetBatteryCoolToTemperature_startUnder(self):
    with self.assertCalls(
        (self.call.battery.GetBatteryInfo(), {'temperature': '500'})):
      self.battery.LetBatteryCoolToTemperature(600)

  @mock.patch('time.sleep', mock.Mock())
  def testLetBatteryCoolToTemperature_startOver(self):
    with self.assertCalls(
        (self.call.battery.GetBatteryInfo(), {'temperature': '500'}),
        (self.call.battery.GetBatteryInfo(), {'temperature': '400'})):
      self.battery.LetBatteryCoolToTemperature(400)

if __name__ == '__main__':
  logging.getLogger().setLevel(logging.DEBUG)
  unittest.main(verbosity=2)
