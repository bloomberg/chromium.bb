#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for pyauto_utils."""

import glob
import os
import shutil
import tempfile
import unittest

import pyauto_utils


class ExistingPathReplacerTest(unittest.TestCase):
  """Tests for ExistingPathReplacer."""

  def setUp(self):
    self._workdir = tempfile.mkdtemp()
    self.assertEqual(0, len(os.listdir(self._workdir)))

  def tearDown(self):
    shutil.rmtree(self._workdir, ignore_errors=True)

  def _CreateFile(self, path):
    fp = open(path, 'w')
    fp.write('magic')
    fp.close()

  def _IsOrigFile(self, path):
    if not os.path.isfile(path):
      return False
    return open(path).read() == 'magic'

  def testNonExistingFile(self):
    """Test when the requested file does not exist."""
    myfile = os.path.join(self._workdir, 'myfile.txt')
    self.assertFalse(os.path.isfile(myfile))
    r = pyauto_utils.ExistingPathReplacer(myfile, path_type='file')
    self.assertTrue(os.path.isfile(myfile))
    del r
    self.assertEqual(0, len(os.listdir(self._workdir)))

  def testExistingFile(self):
    """Test when the requested file exists."""
    myfile = os.path.join(self._workdir, 'myfile.txt')
    self._CreateFile(myfile)
    self.assertTrue(self._IsOrigFile(myfile))
    r = pyauto_utils.ExistingPathReplacer(myfile, path_type='file')
    self.assertFalse(self._IsOrigFile(myfile))
    self.assertEqual(2, len(os.listdir(self._workdir)))
    del r
    self.assertEqual(1, len(os.listdir(self._workdir)))
    self.assertTrue(self._IsOrigFile(myfile))

  def testNonExistingDir(self):
    """Test when the requested dir does not exist."""
    mydir = os.path.join(self._workdir, 'mydir')
    self.assertFalse(os.path.isdir(mydir))
    r = pyauto_utils.ExistingPathReplacer(mydir, path_type='dir')
    self.assertTrue(os.path.isdir(mydir))
    self.assertEqual(0, len(os.listdir(mydir)))
    del r
    self.assertFalse(os.path.isdir(mydir))

  def testExistingDir(self):
    """Test when the requested dir exists."""
    # Create a dir with one file
    mydir = os.path.join(self._workdir, 'mydir')
    os.makedirs(mydir)
    self.assertEqual(1, len(os.listdir(self._workdir)))
    myfile = os.path.join(mydir, 'myfile.txt')
    open(myfile, 'w').close()
    self.assertTrue(os.path.isfile(myfile))
    r = pyauto_utils.ExistingPathReplacer(mydir)
    self.assertEqual(2, len(os.listdir(self._workdir)))
    self.assertFalse(os.path.isfile(myfile))
    del r
    self.assertEqual(1, len(os.listdir(self._workdir)))
    self.assertTrue(os.path.isfile(myfile))


if __name__ == '__main__':
  unittest.main()
