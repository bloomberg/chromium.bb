#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for the contents of device_temp_file.py.
"""

import logging
import os
import sys
import unittest

from pylib import constants
from pylib.utils import device_temp_file

sys.path.append(os.path.join(
    constants.DIR_SOURCE_ROOT, 'third_party', 'pymock'))
import mock # pylint: disable=F0401

class DeviceTempFileTest(unittest.TestCase):

  def setUp(self):
    self.device_utils = mock.MagicMock()

  def testTempFileNameAlreadyExists(self):
    self.device_utils.GetExternalStoragePath.return_value = '/sdcard'
    self.device_utils.FileExists.side_effect = [True, True, True, False]

    tmpfile = device_temp_file.DeviceTempFile(self.device_utils)
    logging.debug('Temp file name: %s' % tmpfile.name)
    self.assertEqual(self.device_utils.FileExists.call_count, 4)
    self.device_utils.WriteFile.assert_called_with(tmpfile.name, '')

  def testTempFileLifecycle(self):
    self.device_utils.GetExternalStoragePath.return_value = '/sdcard'
    self.device_utils.FileExists.return_value = False

    with device_temp_file.DeviceTempFile(self.device_utils) as tmpfile:
      filename = tmpfile.name
      self.assertEqual(self.device_utils.WriteFile.call_count, 1)
      self.assertNotEqual(self.device_utils.RunShellCommand.call_args,
          mock.call(['rm', mock.ANY]))

    self.assertEqual(self.device_utils.RunShellCommand.call_args,
        mock.call(['rm', filename]))

if __name__ == '__main__':
  logging.getLogger().setLevel(logging.DEBUG)
  unittest.main(verbosity=2)
