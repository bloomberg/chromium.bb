#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the logging_config module."""

import mox
import os
import unittest

import fixup_path
fixup_path.FixupPath()

from chromite.lib.paygen import logging_config
from chromite.lib.paygen import unittest_lib


class TestLoggingConfig(unittest_lib.MoxTestCase):
  """Test logging_config module."""

  fake_gmt_default_str = '20131021-215215'
  fake_gmt = logging_config.time.strptime(fake_gmt_default_str, '%Y%m%d-%H%M%S')

  def TestArchiveLog(self, log_path, archive_dir, **kwargs):
    self.mox.StubOutWithMock(logging_config.time, 'gmtime')
    self.mox.StubOutWithMock(logging_config.urilib, 'Copy')

    # Set up the test replay script.
    logging_config.time.gmtime().AndReturn(self.fake_gmt)
    logging_config.urilib.Copy(log_path, mox.IgnoreArg())
    self.mox.ReplayAll()

    # Run the test verification.
    result = logging_config.ArchiveLog(log_path, archive_dir, **kwargs)
    self.mox.VerifyAll()

    return result

  def testArchiveLogDefaults(self):
    log_path = '/some/path/to/my/logfile.log'
    archive_dir = '/some/path/to/archive/logdir'
    archive_basename = 'logfile-%s-UTC.log' % self.fake_gmt_default_str
    self.assertEqual(self.TestArchiveLog(log_path, archive_dir),
                     os.path.join(archive_dir, archive_basename))

  def testArchiveLogBasenamePlain(self):
    log_path = '/some/path/to/my/logfile.log'
    archive_dir = '/some/path/to/archive/logdir'
    archive_basename = 'basename_no_strftime'
    self.assertEqual(self.TestArchiveLog(log_path, archive_dir,
                                         archive_basename=archive_basename),
                     os.path.join(archive_dir, archive_basename))

  def testArchiveLogBasenameTimestamp(self):
    log_path = '/some/path/to/my/logfile.log'
    archive_dir = '/some/path/to/archive/logdir'
    archive_basename_fmt = 'basename_%Y.%m.%d_UTC_%s.txt'
    archive_basename = 'basename_2013.10.21_UTC_1382417535.txt'
    self.assertEqual(self.TestArchiveLog(log_path, archive_dir,
                                         archive_basename=archive_basename_fmt),
                     os.path.join(archive_dir, archive_basename))


if __name__ == '__main__':
  unittest.main()
