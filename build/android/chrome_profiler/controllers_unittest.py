# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from chrome_profiler import profiler

from pylib import android_commands
from pylib.device import device_utils


class BaseControllerTest(unittest.TestCase):
  def setUp(self):
    devices = android_commands.GetAttachedDevices()
    self.browser = 'stable'
    self.package_info = profiler.GetSupportedBrowsers()[self.browser]
    self.device = device_utils.DeviceUtils(devices[0])

    adb = android_commands.AndroidCommands(devices[0])
    adb.StartActivity(self.package_info.package,
                      self.package_info.activity,
                      wait_for_completion=True)
