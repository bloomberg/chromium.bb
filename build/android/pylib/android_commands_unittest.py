# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys
import unittest

sys.path.append(os.path.join(os.pardir, os.path.dirname(__file__)))

from pylib import android_commands

# pylint: disable=W0212,W0702

class TestGetFilesChanged(unittest.TestCase):

  def setUp(self):
    if not os.getenv('BUILDTYPE'):
      os.environ['BUILDTYPE'] = 'Debug'

    devices = android_commands.GetAttachedDevices()
    self.assertGreater(len(devices), 0, 'No device attached!')
    self.ac = android_commands.AndroidCommands(device=devices[0])
    self.host_data_dir = os.path.realpath('test_push_data')
    self.device_data_dir = '%s/test_push_data' % (
        self.ac.RunShellCommand('realpath %s' %
            self.ac.GetExternalStorage())[0])

    os.mkdir(self.host_data_dir)
    for i in xrange(1, 10):
      with open('%s/%d.txt' % (self.host_data_dir, i), 'w') as f:
        f.write('file #%d' % i)

    self.ac.RunShellCommand('mkdir %s' % self.device_data_dir)

  def testGetFilesChangedAllNeeded(self):
    """ Tests GetFilesChanged when none of the files are on the device.
    """
    expected = [('%s/%d.txt' % (self.host_data_dir, i),
                 '%s/%d.txt' % (self.device_data_dir, i))
                for i in xrange(1, 10)]
    actual = self.ac.GetFilesChanged(self.host_data_dir, self.device_data_dir)
    self.assertSequenceEqual(expected, actual)

  def testGetFilesChangedSomeIdentical(self):
    """ Tests GetFilesChanged when some of the files are on the device.
    """
    for i in xrange(1, 5):
      self.ac._adb.Push('%s/%d.txt' % (self.host_data_dir, i),
                        self.device_data_dir)
    expected = [('%s/%d.txt' % (self.host_data_dir, i),
                 '%s/%d.txt' % (self.device_data_dir, i))
                for i in xrange(5, 10)]
    actual = self.ac.GetFilesChanged(self.host_data_dir, self.device_data_dir)
    self.assertSequenceEqual(expected, actual)

  def testGetFilesChangedAllIdentical(self):
    """ Tests GetFilesChanged when all of the files are on the device.
    """
    for i in xrange(1, 10):
      self.ac._adb.Push('%s/%d.txt' % (self.host_data_dir, i),
                        self.device_data_dir)
    expected = []
    actual = self.ac.GetFilesChanged(self.host_data_dir, self.device_data_dir)
    self.assertSequenceEqual(expected, actual)

  def testGetFilesChangedRename(self):
    """ Tests GetFilesChanged when one of the files has been renamed.

        This tests both with and without the ignore_filenames flag set.
    """
    for i in xrange(5, 10):
      self.ac._adb.Push('%s/%d.txt' % (self.host_data_dir, i),
                        self.device_data_dir)
    os.rename('%s/5.txt' % (self.host_data_dir),
              '%s/99.txt' % (self.host_data_dir))

    expected = [('%s/%d.txt' % (self.host_data_dir, i),
                 '%s/%d.txt' % (self.device_data_dir, i))
                for i in xrange(1, 5)]
    actual = self.ac.GetFilesChanged(self.host_data_dir, self.device_data_dir,
                                     ignore_filenames=True)
    self.assertSequenceEqual(expected, actual)

    expected.append(('%s/99.txt' % self.host_data_dir,
                     '%s/99.txt' % self.device_data_dir))
    actual = self.ac.GetFilesChanged(self.host_data_dir, self.device_data_dir)
    self.assertSequenceEqual(expected, actual)

  def testGetFilesChangedCopy(self):
    """ Tests GetFilesChanged when one of the files has been copied.

        This tests both with and without the ignore_filenames flag set.
    """
    for i in xrange(5, 10):
      self.ac._adb.Push('%s/%d.txt' % (self.host_data_dir, i),
                        self.device_data_dir)
    shutil.copy('%s/5.txt' % self.host_data_dir,
                '%s/99.txt' % self.host_data_dir)

    expected = [('%s/%d.txt' % (self.host_data_dir, i),
                 '%s/%d.txt' % (self.device_data_dir, i))
                for i in xrange(1, 5)]
    actual = self.ac.GetFilesChanged(self.host_data_dir, self.device_data_dir,
                                     ignore_filenames=True)
    self.assertSequenceEqual(expected, actual)

    expected.append(('%s/99.txt' % self.host_data_dir,
                     '%s/99.txt' % self.device_data_dir))
    actual = self.ac.GetFilesChanged(self.host_data_dir, self.device_data_dir)
    self.assertSequenceEqual(expected, actual)

  def testGetFilesChangedIndividual(self):
    """ Tests GetFilesChanged when provided one file.
    """
    expected = [('%s/1.txt' % self.host_data_dir,
                 '%s/1.txt' % self.device_data_dir)]
    actual = self.ac.GetFilesChanged('%s/1.txt' % self.host_data_dir,
                                     '%s/1.txt' % self.device_data_dir)
    self.assertSequenceEqual(expected, actual)

  def testGetFilesChangedFileToDirectory(self):
    """ Tests GetFilesChanged when provided a file from the host and a
        directory on the device.
    """
    expected = [('%s/1.txt' % self.host_data_dir,
                 '%s' % self.device_data_dir)]
    actual = self.ac.GetFilesChanged('%s/1.txt' % self.host_data_dir,
                                     '%s' % self.device_data_dir)
    self.assertSequenceEqual(expected, actual)

  def tearDown(self):
    try:
      shutil.rmtree(self.host_data_dir)
      self.ac.RunShellCommand('rm -rf %s' % self.device_data_dir)
    except:
      pass

if __name__ == '__main__':
  unittest.main()

