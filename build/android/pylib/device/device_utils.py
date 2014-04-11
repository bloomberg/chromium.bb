# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Provides a variety of device interactions based on adb.

Eventually, this will be based on adb_wrapper.
"""

import pylib.android_commands
from pylib.device import adb_wrapper


def GetAVDs():
  return pylib.android_commands.GetAVDs()


class DeviceUtils(object):

  def __init__(self, device):
    self.old_interface = None
    if isinstance(device, basestring):
      self.old_interface = pylib.android_commands.AndroidCommands(device)
    elif isinstance(device, adb_wrapper.AdbWrapper):
      self.old_interface = pylib.android_commands.AndroidCommands(str(device))
    elif isinstance(device, pylib.android_commands.AndroidCommands):
      self.old_interface = device
    elif not device:
      self.old_interface = pylib.android_commands.AndroidCommands()

