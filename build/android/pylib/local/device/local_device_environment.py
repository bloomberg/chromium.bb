# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from pylib.base import environment
from pylib.device import adb_wrapper
from pylib.device import device_errors
from pylib.device import device_utils
from pylib.utils import parallelizer


class LocalDeviceEnvironment(environment.Environment):

  def __init__(self, args, _error_func):
    super(LocalDeviceEnvironment, self).__init__()
    self._device = args.test_device
    self._devices = []
    self._max_tries = 1 + args.num_retries
    self._tool_name = args.tool

  #override
  def SetUp(self):
    # TODO(jbudorick): This can be refined to support filters etc.
    available_devices = adb_wrapper.AdbWrapper.GetDevices()
    if not available_devices:
      raise device_errors.NoDevicesError
    if self._device:
      if self._device not in available_devices:
        raise device_errors.DeviceUnreachableError(
            'Could not find device %r' % self._device)
      self._devices = [device_utils.DeviceUtils(self._device)]
    else:
      self._devices = [
          device_utils.DeviceUtils(s)
          for s in available_devices]

  @property
  def devices(self):
    return self._devices

  @property
  def parallel_devices(self):
    return parallelizer.SyncParallelizer(self._devices)

  @property
  def max_tries(self):
    return self._max_tries

  @property
  def tool(self):
    return self._tool_name

  #override
  def TearDown(self):
    pass

