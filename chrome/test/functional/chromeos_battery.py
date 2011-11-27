#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # Must be imported before pyauto
import pyauto

from chromeos.power_strip import PowerStrip

class ChromeosBattery(pyauto.PyUITest):
  """Tests ChromeOS Battery Status.

  Preconditions:
     1) Device under test (DUT) is connected to the LAN via an
     Ethernet-to-USB adapter plugged into one of its USB ports.
     2) AC power cable is connected to the DUT, and plugged into
     the IP controlled Power Switch, outlet #4, located in the lab.
     3) Battery is installed in the DUT, and battery is not fully
     discharged.
     4) Tester should have physical access to the power switch.

  Note about time calculation:
     When AC power is turned off or on, the battery will take from 2
     to 60 seconds to calculate the time remaining to empty or full.
     While calculating, the keys 'battery_time_to_full' and
     'battery_time_to_empty' are absent.
  """

  _BATTERY_CONFIG_FILE = os.path.join(pyauto.PyUITest.DataDir(),
                                      'pyauto_private', 'chromeos', 'power',
                                      'battery_testbed_config')

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    self.InitPowerStrip()

  def tearDown(self):
    # Leave AC power ON so battery does not discharge between tests
    self._power_strip.PowerOn(self._outlet_battery_full)
    pyauto.PyUITest.tearDown(self)

  def InitPowerStrip(self):
    assert os.path.exists(ChromeosBattery._BATTERY_CONFIG_FILE), \
        'Power Strip configuration file does not exist.'
    power_config = pyauto.PyUITest.EvalDataFrom(
        ChromeosBattery._BATTERY_CONFIG_FILE)
    self._power_strip = PowerStrip(power_config['strip_ip'])
    self._outlet_battery_full = (power_config['configs']
                                             ['battery_full']
                                             ['outlet_id'])

  def WaitUntilBatteryState(self, ac_power_on, time_key):
    assert self.WaitUntil(lambda: self.BatteryPowerAndChargeStateAgree(
        ac_power_on, time_key), timeout=60, retry_sleep=1), \
        'Battery charge/discharge time was not calculated.'
    return

  def BatteryPowerAndChargeStateAgree(self, ac_power_on, time_key):
    battery_status = self.GetBatteryInfo()
    return (battery_status.get('line_power_on') == ac_power_on and
        time_key in battery_status)

  def testBatteryChargesWhenACisOn(self):
    """Calculate battery time to full when AC is ON"""
    # Apply AC power to chromebook with battery.
    self._power_strip.PowerOn(self._outlet_battery_full)

    # Get info about charging battery
    self.WaitUntilBatteryState(True,'battery_time_to_full')
    battery_status = self.GetBatteryInfo()
    self.assertTrue(battery_status.get('battery_is_present'),
        msg='Battery is not present.')
    self.assertTrue(battery_status.get('line_power_on'),
        msg='Line power is off.')
    self.assertTrue(battery_status.get('battery_time_to_full') >= 0,
        msg='Battery charge time is negative.')

  def testBatteryDischargesWhenACisOff(self):
    """Calculate battery time to empty when AC is OFF"""
    self._power_strip.PowerOff(self._outlet_battery_full)

    # Get info about discharging battery
    self.WaitUntilBatteryState(False,'battery_time_to_empty')
    battery_status = self.GetBatteryInfo()
    self.assertTrue(battery_status.get('battery_is_present'),
        msg='Battery is not present.')
    self.assertFalse(battery_status.get('line_power_on'),
        msg='Line power is on.')
    self.assertTrue(battery_status.get('battery_time_to_empty') >= 0,
        msg='Battery discharge time is negative.')

  def testBatteryTimesAreDifferent(self):
    """Time to full and time to empty should be different"""
    # Turn AC Power ON
    self._power_strip.PowerOn(self._outlet_battery_full)

    # Get charging battery time to full
    self.WaitUntilBatteryState(True,'battery_time_to_full')
    battery_status = self.GetBatteryInfo()
    time_to_full = battery_status.get('battery_time_to_full')

    # Turn AC Power OFF
    self._power_strip.PowerOff(self._outlet_battery_full)

    # Get discharging battery time to empty
    self.WaitUntilBatteryState(False,'battery_time_to_empty')
    battery_status = self.GetBatteryInfo()
    time_to_empty = battery_status.get('battery_time_to_empty')

    # Compare charge to discharge time
    """Confirm that time to full and time to empty are not
       returning the same value, but that the values are
       different (e.g., both are not zero)."""
    self.assertNotEqual(time_to_full, time_to_empty,
        msg='Battery time to full equals time to empty. '
            'Though very unlikely, this is not impossible. '
            'If test failed falsely, Kris owes Scott a beer.')


if __name__ == '__main__':
  pyauto_functional.Main()
