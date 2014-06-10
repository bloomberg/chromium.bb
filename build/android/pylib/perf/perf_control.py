# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time

from pylib import android_commands
from pylib.device import device_utils


class PerfControl(object):
  """Provides methods for setting the performance mode of a device."""
  _SCALING_GOVERNOR_FMT = (
      '/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor')
  _KERNEL_MAX = '/sys/devices/system/cpu/kernel_max'

  def __init__(self, device):
    # TODO(jbudorick) Remove once telemetry gets switched over.
    if isinstance(device, android_commands.AndroidCommands):
      device = device_utils.DeviceUtils(device)
    self._device = device
    kernel_max = self._device.old_interface.GetFileContents(
        PerfControl._KERNEL_MAX, log_result=False)
    assert kernel_max, 'Unable to find %s' % PerfControl._KERNEL_MAX
    self._kernel_max = int(kernel_max[0])
    logging.info('Maximum CPU index: %d', self._kernel_max)
    self._original_scaling_governor = \
        self._device.old_interface.GetFileContents(
            PerfControl._SCALING_GOVERNOR_FMT % 0,
            log_result=False)[0]

  def SetHighPerfMode(self):
    """Sets the highest possible performance mode for the device."""
    self._SetScalingGovernorInternal('performance')

  def SetDefaultPerfMode(self):
    """Sets the performance mode for the device to its default mode."""
    product_model = self._device.old_interface.GetProductModel()
    governor_mode = {
        'GT-I9300': 'pegasusq',
        'Galaxy Nexus': 'interactive',
        'Nexus 4': 'ondemand',
        'Nexus 7': 'interactive',
        'Nexus 10': 'interactive'
    }.get(product_model, 'ondemand')
    self._SetScalingGovernorInternal(governor_mode)

  def RestoreOriginalPerfMode(self):
    """Resets the original performance mode of the device."""
    self._SetScalingGovernorInternal(self._original_scaling_governor)

  def _SetScalingGovernorInternal(self, value):
    for cpu in range(self._kernel_max + 1):
      scaling_governor_file = PerfControl._SCALING_GOVERNOR_FMT % cpu
      if self._device.old_interface.FileExistsOnDevice(scaling_governor_file):
        logging.info('Writing scaling governor mode \'%s\' -> %s',
                     value, scaling_governor_file)
        self._device.old_interface.SetProtectedFileContents(
            scaling_governor_file, value)

  def ForceAllCpusOnline(self, force_online):
    """Force all CPUs on a device to be online.

    Force every CPU core on an Android device to remain online, or return the
    cores under system power management control. This is needed to work around
    a bug in perf which makes it unable to record samples from CPUs that become
    online when recording is already underway.

    Args:
      force_online: True to set all CPUs online, False to return them under
          system power management control.
    """
    def ForceCpuOnline(online_path):
      script = 'chmod 644 {0}; echo 1 > {0}; chmod 444 {0}'.format(online_path)
      self._device.old_interface.RunShellCommandWithSU(script)
      return self._device.old_interface.GetFileContents(online_path)[0] == '1'

    def ResetCpu(online_path):
      self._device.old_interface.RunShellCommandWithSU(
          'chmod 644 %s' % online_path)

    def WaitFor(condition):
      for _ in range(100):
        if condition():
          return
        time.sleep(0.1)
      raise RuntimeError('Timed out')

    cpu_online_files = self._device.old_interface.RunShellCommand(
        'ls -d /sys/devices/system/cpu/cpu[0-9]*/online')
    for online_path in cpu_online_files:
      if force_online:
        WaitFor(lambda: ForceCpuOnline(online_path))
      else:
        ResetCpu(online_path)
