# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pylib import android_commands
from pylib.device import device_utils

class CacheControl(object):
  _DROP_CACHES = '/proc/sys/vm/drop_caches'

  def __init__(self, device):
    # TODO(jbudorick) Remove once telemetry gets switched over.
    assert not isinstance(device, android_commands.AndroidCommands)
    self._device = device

  def DropRamCaches(self):
    """Drops the filesystem ram caches for performance testing."""
    self._device.RunShellCommand('sync', as_root=True)
    self._device.WriteFile(CacheControl._DROP_CACHES, '3', as_root=True)

