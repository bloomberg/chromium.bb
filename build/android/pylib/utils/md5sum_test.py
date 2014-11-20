#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

from pylib import cmd_helper
from pylib import constants
from pylib.utils import md5sum

sys.path.append(
    os.path.join(constants.DIR_SOURCE_ROOT, 'third_party', 'pymock'))
import mock

TEST_OUT_DIR = os.path.join('test', 'out', 'directory')
HOST_MD5_EXECUTABLE = os.path.join(TEST_OUT_DIR, 'md5sum_bin_host')

class Md5SumTest(unittest.TestCase):

  def setUp(self):
    self._patchers = [
        mock.patch('pylib.constants.GetOutDirectory',
                   new=mock.Mock(return_value=TEST_OUT_DIR)),
    ]
    for p in self._patchers:
      p.start()

  def tearDown(self):
    for p in self._patchers:
      p.stop()

  def testCalculateHostMd5Sums_singlePath(self):
    test_path = '/test/host/file.dat'
    mock_get_cmd_output = mock.Mock(
        return_value='0123456789abcdeffedcba9876543210 /test/host/file.dat')
    with mock.patch('pylib.cmd_helper.GetCmdOutput', new=mock_get_cmd_output):
      out = md5sum.CalculateHostMd5Sums(test_path)
      self.assertEquals(1, len(out))
      self.assertEquals('0123456789abcdeffedcba9876543210', out[0].hash)
      self.assertEquals('/test/host/file.dat', out[0].path)
      mock_get_cmd_output.assert_called_once_with(
          [HOST_MD5_EXECUTABLE, '/test/host/file.dat'])

  def testCalculateHostMd5Sums_list(self):
    test_paths = ['/test/host/file0.dat', '/test/host/file1.dat']
    mock_get_cmd_output = mock.Mock(
        return_value='0123456789abcdeffedcba9876543210 /test/host/file0.dat\n'
                     '123456789abcdef00fedcba987654321 /test/host/file1.dat\n')
    with mock.patch('pylib.cmd_helper.GetCmdOutput', new=mock_get_cmd_output):
      out = md5sum.CalculateHostMd5Sums(test_paths)
      self.assertEquals(2, len(out))
      self.assertEquals('0123456789abcdeffedcba9876543210', out[0].hash)
      self.assertEquals('/test/host/file0.dat', out[0].path)
      self.assertEquals('123456789abcdef00fedcba987654321', out[1].hash)
      self.assertEquals('/test/host/file1.dat', out[1].path)
      mock_get_cmd_output.assert_called_once_with(
          [HOST_MD5_EXECUTABLE, '/test/host/file0.dat',
           '/test/host/file1.dat'])

  def testCalculateHostMd5Sums_generator(self):
    test_paths = ('/test/host/' + p for p in ['file0.dat', 'file1.dat'])
    mock_get_cmd_output = mock.Mock(
        return_value='0123456789abcdeffedcba9876543210 /test/host/file0.dat\n'
                     '123456789abcdef00fedcba987654321 /test/host/file1.dat\n')
    with mock.patch('pylib.cmd_helper.GetCmdOutput', new=mock_get_cmd_output):
      out = md5sum.CalculateHostMd5Sums(test_paths)
      self.assertEquals(2, len(out))
      self.assertEquals('0123456789abcdeffedcba9876543210', out[0].hash)
      self.assertEquals('/test/host/file0.dat', out[0].path)
      self.assertEquals('123456789abcdef00fedcba987654321', out[1].hash)
      self.assertEquals('/test/host/file1.dat', out[1].path)
      mock_get_cmd_output.assert_called_once_with(
          [HOST_MD5_EXECUTABLE, '/test/host/file0.dat', '/test/host/file1.dat'])

  def testCalculateDeviceMd5Sums_singlePath(self):
    test_path = '/storage/emulated/legacy/test/file.dat'

    device = mock.NonCallableMock()
    device.adb = mock.NonCallableMock()
    device.adb.Push = mock.Mock()
    device_md5sum_output = [
        '0123456789abcdeffedcba9876543210 '
            '/storage/emulated/legacy/test/file.dat',
    ]
    device.RunShellCommand = mock.Mock(return_value=device_md5sum_output)

    mock_temp_file = mock.mock_open()
    mock_temp_file.return_value.name = '/tmp/test/script/file.sh'

    mock_device_temp_file = mock.mock_open()
    mock_device_temp_file.return_value.name = (
        '/data/local/tmp/test/script/file.sh')

    with mock.patch('tempfile.NamedTemporaryFile', new=mock_temp_file), (
         mock.patch('pylib.utils.device_temp_file.DeviceTempFile',
                    new=mock_device_temp_file)):
      out = md5sum.CalculateDeviceMd5Sums(test_path, device)
      self.assertEquals(1, len(out))
      self.assertEquals('0123456789abcdeffedcba9876543210', out[0].hash)
      self.assertEquals('/storage/emulated/legacy/test/file.dat', out[0].path)
      device.adb.Push.assert_called_once_with(
          '/tmp/test/script/file.sh', '/data/local/tmp/test/script/file.sh')
      device.RunShellCommand.assert_called_once_with(
          ['sh', '/data/local/tmp/test/script/file.sh'])

  def testCalculateDeviceMd5Sums_list(self):
    test_path = ['/storage/emulated/legacy/test/file0.dat',
                 '/storage/emulated/legacy/test/file1.dat']
    device = mock.NonCallableMock()
    device.adb = mock.NonCallableMock()
    device.adb.Push = mock.Mock()
    device_md5sum_output = [
        '0123456789abcdeffedcba9876543210 '
            '/storage/emulated/legacy/test/file0.dat',
        '123456789abcdef00fedcba987654321 '
            '/storage/emulated/legacy/test/file1.dat',
    ]
    device.RunShellCommand = mock.Mock(return_value=device_md5sum_output)

    mock_temp_file = mock.mock_open()
    mock_temp_file.return_value.name = '/tmp/test/script/file.sh'

    mock_device_temp_file = mock.mock_open()
    mock_device_temp_file.return_value.name = (
        '/data/local/tmp/test/script/file.sh')

    with mock.patch('tempfile.NamedTemporaryFile', new=mock_temp_file), (
         mock.patch('pylib.utils.device_temp_file.DeviceTempFile',
                    new=mock_device_temp_file)):
      out = md5sum.CalculateDeviceMd5Sums(test_path, device)
      self.assertEquals(2, len(out))
      self.assertEquals('0123456789abcdeffedcba9876543210', out[0].hash)
      self.assertEquals('/storage/emulated/legacy/test/file0.dat', out[0].path)
      self.assertEquals('123456789abcdef00fedcba987654321', out[1].hash)
      self.assertEquals('/storage/emulated/legacy/test/file1.dat', out[1].path)
      device.adb.Push.assert_called_once_with(
          '/tmp/test/script/file.sh', '/data/local/tmp/test/script/file.sh')
      device.RunShellCommand.assert_called_once_with(
          ['sh', '/data/local/tmp/test/script/file.sh'])

  def testCalculateDeviceMd5Sums_generator(self):
    test_path = ('/storage/emulated/legacy/test/file%d.dat' % n
                 for n in xrange(0, 2))

    device = mock.NonCallableMock()
    device.adb = mock.NonCallableMock()
    device.adb.Push = mock.Mock()
    device_md5sum_output = [
        '0123456789abcdeffedcba9876543210 '
            '/storage/emulated/legacy/test/file0.dat',
        '123456789abcdef00fedcba987654321 '
            '/storage/emulated/legacy/test/file1.dat',
    ]
    device.RunShellCommand = mock.Mock(return_value=device_md5sum_output)

    mock_temp_file = mock.mock_open()
    mock_temp_file.return_value.name = '/tmp/test/script/file.sh'

    mock_device_temp_file = mock.mock_open()
    mock_device_temp_file.return_value.name = (
        '/data/local/tmp/test/script/file.sh')

    with mock.patch('tempfile.NamedTemporaryFile', new=mock_temp_file), (
         mock.patch('pylib.utils.device_temp_file.DeviceTempFile',
                    new=mock_device_temp_file)):
      out = md5sum.CalculateDeviceMd5Sums(test_path, device)
      self.assertEquals(2, len(out))
      self.assertEquals('0123456789abcdeffedcba9876543210', out[0].hash)
      self.assertEquals('/storage/emulated/legacy/test/file0.dat', out[0].path)
      self.assertEquals('123456789abcdef00fedcba987654321', out[1].hash)
      self.assertEquals('/storage/emulated/legacy/test/file1.dat', out[1].path)
      device.adb.Push.assert_called_once_with(
          '/tmp/test/script/file.sh', '/data/local/tmp/test/script/file.sh')
      device.RunShellCommand.assert_called_once_with(
          ['sh', '/data/local/tmp/test/script/file.sh'])


if __name__ == '__main__':
  unittest.main(verbosity=2)

