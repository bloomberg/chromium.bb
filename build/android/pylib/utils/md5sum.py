# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import logging
import os
import tempfile
import types

from pylib import cmd_helper
from pylib import constants
from pylib.utils import device_temp_file

HashAndPath = collections.namedtuple('HashAndPath', ['hash', 'path'])

MD5SUM_DEVICE_LIB_PATH = '/data/local/tmp/md5sum/'
MD5SUM_DEVICE_BIN_PATH = MD5SUM_DEVICE_LIB_PATH + 'md5sum_bin'

MD5SUM_DEVICE_SCRIPT_FORMAT = (
    'test -f {path} -o -d {path} '
    '&& LD_LIBRARY_PATH={md5sum_lib} {md5sum_bin} {path}')


def CalculateHostMd5Sums(paths):
  """Calculates the MD5 sum value for all items in |paths|.

  Args:
    paths: A list of host paths to md5sum.
  Returns:
    A list of named tuples with 'hash' and 'path' attributes.
  """
  if isinstance(paths, basestring):
    paths = [paths]

  out = cmd_helper.GetCmdOutput(
      [os.path.join(constants.GetOutDirectory(), 'md5sum_bin_host')] +
          [p for p in paths])
  return [HashAndPath(*l.split(None, 1)) for l in out.splitlines()]


def CalculateDeviceMd5Sums(paths, device):
  """Calculates the MD5 sum value for all items in |paths|.

  Args:
    paths: A list of device paths to md5sum.
  Returns:
    A list of named tuples with 'hash' and 'path' attributes.
  """
  if isinstance(paths, basestring):
    paths = [paths]

  if not device.FileExists(MD5SUM_DEVICE_BIN_PATH):
    device.adb.Push(
        os.path.join(constants.GetOutDirectory(), 'md5sum_dist'),
        MD5SUM_DEVICE_LIB_PATH)

  out = []
  with tempfile.NamedTemporaryFile() as md5sum_script_file:
    with device_temp_file.DeviceTempFile(device) as md5sum_device_script_file:
      md5sum_script = (
          MD5SUM_DEVICE_SCRIPT_FORMAT.format(
              path=p, md5sum_lib=MD5SUM_DEVICE_LIB_PATH,
              md5sum_bin=MD5SUM_DEVICE_BIN_PATH)
          for p in paths)
      md5sum_script_file.write('; '.join(md5sum_script))
      md5sum_script_file.flush()
      device.adb.Push(md5sum_script_file.name, md5sum_device_script_file.name)
      out = device.RunShellCommand(['sh', md5sum_device_script_file.name])

  return [HashAndPath(*l.split(None, 1)) for l in out]

