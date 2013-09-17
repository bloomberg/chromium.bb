# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import logging


class PerfControl(object):
  """Provides methods for setting the performance mode of a device."""
  _SCALING_GOVERNOR_FMT = (
      '/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor')
  _KERNEL_MAX = '/sys/devices/system/cpu/kernel_max'

  def __init__(self, adb):
    self._adb = adb
    kernel_max = self._adb.GetFileContents(PerfControl._KERNEL_MAX,
                                           log_result=False)
    assert kernel_max, 'Unable to find %s' % PerfControl._KERNEL_MAX
    self._kernel_max = int(kernel_max[0])
    logging.info('Maximum CPU index: %d', self._kernel_max)
    self._original_scaling_governor = self._adb.GetFileContents(
        PerfControl._SCALING_GOVERNOR_FMT % 0,
        log_result=False)[0]

  def SetHighPerfMode(self):
    """Sets the highest possible performance mode for the device."""
    self._SetScalingGovernorInternal('performance')

  def SetDefaultPerfMode(self):
    """Sets the performance mode for the device to its default mode."""
    product_model = self._adb.GetProductModel()
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
      if self._adb.FileExistsOnDevice(scaling_governor_file):
        logging.info('Writing scaling governor mode \'%s\' -> %s',
                     value, scaling_governor_file)
        self._adb.SetProtectedFileContents(scaling_governor_file, value)
