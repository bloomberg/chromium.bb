# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A temp file that automatically gets pushed and deleted from a device."""

# pylint: disable=W0622

import random
import time

class DeviceTempFile(object):
  def __init__(self, device, prefix='temp_file', suffix=''):
    """Find an unused temporary file path in the devices external directory.

    When this object is closed, the file will be deleted on the device.

    Args:
      device: An instance of DeviceUtils
      prefix: The prefix of the name of the temp file.
      suffix: The suffix of the name of the temp file.
    """
    self._device = device
    while True:
      i = random.randint(0, 1000000)
      self.name = '%s/%s-%d-%010d%s' % (
          self._device.GetExternalStoragePath(),
          prefix, int(time.time()), i, suffix)
      if not self._device.FileExists(self.name):
        break
    # Immediately create an empty file so that other temp files can't
    # be given the same name.
    # |as_root| must be set to False due to the implementation of |WriteFile|.
    # Having |as_root| be True may cause infinite recursion.
    self._device.WriteFile(self.name, '', as_root=False)

  def close(self):
    """Deletes the temporary file from the device."""
    self._device.RunShellCommand(['rm', self.name])

  def __enter__(self):
    return self

  def __exit__(self, type, value, traceback):
    self.close()
