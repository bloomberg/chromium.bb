# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from pylib import android_commands
from pylib.device import device_utils


class PerfControl(object):
  """Provides methods for setting the performance mode of a device."""
  _SCALING_GOVERNOR_FMT = (
      '/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor')
  _CPU_ONLINE_FMT = '/sys/devices/system/cpu/cpu%d/online'
  _KERNEL_MAX = '/sys/devices/system/cpu/kernel_max'

  def __init__(self, device):
    # TODO(jbudorick) Remove once telemetry gets switched over.
    if isinstance(device, android_commands.AndroidCommands):
      device = device_utils.DeviceUtils(device)
    self._device = device
    cpu_files = self._device.RunShellCommand(
      'ls -d /sys/devices/system/cpu/cpu[0-9]*')
    self._num_cpu_cores = len(cpu_files)
    assert self._num_cpu_cores > 0, 'Failed to detect CPUs.'
    logging.info('Number of CPUs: %d', self._num_cpu_cores)
    self._have_mpdecision = self._device.FileExists('/system/bin/mpdecision')

  def SetHighPerfMode(self):
    # TODO(epenner): Enable on all devices (http://crbug.com/383566)
    if 'Nexus 4' == self._device.old_interface.GetProductModel():
      self._ForceAllCpusOnline(True)
      if not self._AllCpusAreOnline():
        logging.warning('Failed to force CPUs online. Results may be noisy!')
    self._SetScalingGovernorInternal('performance')

  def SetPerfProfilingMode(self):
    """Sets the highest possible performance mode for the device."""
    self._ForceAllCpusOnline(True)
    self._SetScalingGovernorInternal('performance')
    if not self._AllCpusAreOnline():
      if not self._device.old_interface.IsRootEnabled():
        raise RuntimeError('Need root to force CPUs online.')
      raise RuntimeError('Failed to force CPUs online.')

  def SetDefaultPerfMode(self):
    """Sets the performance mode for the device to its default mode."""
    product_model = self._device.GetProp('ro.product.model')
    governor_mode = {
        'GT-I9300': 'pegasusq',
        'Galaxy Nexus': 'interactive',
        'Nexus 4': 'ondemand',
        'Nexus 7': 'interactive',
        'Nexus 10': 'interactive'
    }.get(product_model, 'ondemand')
    self._SetScalingGovernorInternal(governor_mode)
    self._ForceAllCpusOnline(False)

  def _SetScalingGovernorInternal(self, value):
    for cpu in range(self._num_cpu_cores):
      scaling_governor_file = PerfControl._SCALING_GOVERNOR_FMT % cpu
      if self._device.FileExists(scaling_governor_file):
        logging.info('Writing scaling governor mode \'%s\' -> %s',
                     value, scaling_governor_file)
        self._device.WriteFile(scaling_governor_file, value, as_root=True)

  def _AllCpusAreOnline(self):
    for cpu in range(self._num_cpu_cores):
      online_path = PerfControl._CPU_ONLINE_FMT % cpu
      if self._device.ReadFile(online_path)[0] == '0':
        return False
    return True

  def _ForceAllCpusOnline(self, force_online):
    """Enable all CPUs on a device.

    Some vendors (or only Qualcomm?) hot-plug their CPUs, which can add noise
    to measurements:
    - In perf, samples are only taken for the CPUs that are online when the
      measurement is started.
    - The scaling governor can't be set for an offline CPU and frequency scaling
      on newly enabled CPUs adds noise to both perf and tracing measurements.

    It appears Qualcomm is the only vendor that hot-plugs CPUs, and on Qualcomm
    this is done by "mpdecision".

    """
    if self._have_mpdecision:
      script = 'stop mpdecision' if force_online else 'start mpdecision'
      self._device.RunShellCommand(script, as_root=True)

    if not self._have_mpdecision and not self._AllCpusAreOnline():
      logging.warning('Unexpected cpu hot plugging detected.')

    if not force_online:
      return

    for cpu in range(self._num_cpu_cores):
      online_path = PerfControl._CPU_ONLINE_FMT % cpu
      self._device.WriteFile(online_path, '1', as_root=True)
