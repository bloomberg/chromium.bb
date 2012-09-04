#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import doctest
import os
import oshelpers
import shutil
import subprocess
import sys
import tempfile
import unittest
import zipfile


class RunZipError(subprocess.CalledProcessError):
  def __init__(self, retcode, command, output, error_output):
    subprocess.CalledProcessError.__init__(self, retcode, command)
    self.output = output
    self.error_output = error_output

  def __str__(self):
    msg = subprocess.CalledProcessError.__str__(self)
    msg += '.\nstdout: """%s"""' % (self.output,)
    msg += '.\nstderr: """%s"""' % (self.error_output,)
    return msg

def RunZip(args, cwd):
  command = [sys.executable,
             os.path.join(os.path.dirname(__file__), 'oshelpers.py'),
             'zip'] + args
  process = subprocess.Popen(stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE,
                             args=command,
                             cwd=cwd)
  output, error_output = process.communicate()
  retcode = process.poll()

  if retcode != 0:
    raise RunZipError(retcode, command, output, error_output)
  return output, error_output


class TestZip(unittest.TestCase):
  def setUp(self):
    # make zipname -> "testFooBar.zip"
    self.zipname = self.id().split('.')[-1] + '.zip'
    self.zipfile = None
    self.tempdir = tempfile.mkdtemp()
    shutil.copy(os.path.join(os.path.dirname(__file__), 'oshelpers.py'),
        self.tempdir)

  def tearDown(self):
    if self.zipfile:
      self.zipfile.close()
    shutil.rmtree(self.tempdir)

  def GetTempPath(self, basename):
    return os.path.join(self.tempdir, basename)

  def MakeFile(self, rel_path, size):
    with open(os.path.join(self.tempdir, rel_path), 'wb') as f:
      f.write('0' * size)
    return rel_path

  def RunZip(self, *args):
    return RunZip(*args, cwd=self.tempdir)

  def OpenZipFile(self):
    self.zipfile = zipfile.ZipFile(self.GetTempPath(self.zipname), 'r')

  def CloseZipFile(self):
    self.zipfile.close()
    self.zipfile = None

  def GetZipInfo(self, path):
    return self.zipfile.getinfo(oshelpers.OSMakeZipPath(path))


  def testNothingToDo(self):
    self.assertRaises(subprocess.CalledProcessError, self.RunZip,
        [self.zipname, 'nonexistent_file'])
    self.assertFalse(os.path.exists(self.zipname))

  def testAddSomeFiles(self):
    file1 = self.MakeFile('file1', 1024)
    file2 = self.MakeFile('file2', 3354)
    self.RunZip([self.zipname, file1, file2])
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 2)
    self.assertEqual(self.GetZipInfo(file1).file_size, 1024)
    self.assertEqual(self.GetZipInfo(file2).file_size, 3354)
    # make sure files are added in order
    self.assertEqual(self.zipfile.namelist()[0], file1)

  def testAddFilesWithGlob(self):
    self.MakeFile('file1', 1024)
    self.MakeFile('file2', 3354)
    self.RunZip([self.zipname, 'file*'])
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 2)

  def testAddDir(self):
    os.mkdir(self.GetTempPath('dir1'))
    self.RunZip([self.zipname, 'dir1'])
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 1)

  def testAddRecursive(self):
    os.mkdir(self.GetTempPath('dir1'))
    self.MakeFile(os.path.join('dir1', 'file1'), 256)
    os.mkdir(self.GetTempPath(os.path.join('dir1', 'dir2')))
    self.MakeFile(os.path.join('dir1', 'dir2', 'file2'), 1234)
    self.RunZip([self.zipname, '-r', 'dir1'])
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 4)

  def testUpdate(self):
    file1 = self.MakeFile('file1', 1223)
    self.RunZip([self.zipname, file1])
    self.OpenZipFile()
    self.assertEqual(self.GetZipInfo(file1).file_size, 1223)

    file1 = self.MakeFile('file1', 2334)
    self.RunZip([self.zipname, file1])
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 1)
    self.assertEqual(self.GetZipInfo(file1).file_size, 2334)

  def testUpdateOneFileOutOfMany(self):
    file1 = self.MakeFile('file1', 128)
    file2 = self.MakeFile('file2', 256)
    file3 = self.MakeFile('file3', 512)
    file4 = self.MakeFile('file4', 1024)
    self.RunZip([self.zipname, file1, file2, file3, file4])
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 4)
    self.CloseZipFile()

    file3 = self.MakeFile('file3', 768)
    self.RunZip([self.zipname, file3])
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 4)
    self.assertEqual(self.zipfile.namelist()[0], file1)
    self.assertEqual(self.GetZipInfo(file1).file_size, 128)
    self.assertEqual(self.zipfile.namelist()[1], file2)
    self.assertEqual(self.GetZipInfo(file2).file_size, 256)
    self.assertEqual(self.zipfile.namelist()[2], file3)
    self.assertEqual(self.GetZipInfo(file3).file_size, 768)
    self.assertEqual(self.zipfile.namelist()[3], file4)
    self.assertEqual(self.GetZipInfo(file4).file_size, 1024)

  def testUpdateSubdirectory(self):
    os.mkdir(self.GetTempPath('dir1'))
    file1 = self.MakeFile(os.path.join('dir1', 'file1'), 256)
    os.mkdir(self.GetTempPath(os.path.join('dir1', 'dir2')))
    self.MakeFile(os.path.join('dir1', 'dir2', 'file2'), 1234)
    self.RunZip([self.zipname, '-r', 'dir1'])
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 4)
    self.assertEqual(self.GetZipInfo(file1).file_size, 256)
    self.CloseZipFile()

    self.MakeFile(file1, 2560)
    self.RunZip([self.zipname, file1])
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 4)
    self.assertEqual(self.GetZipInfo(file1).file_size, 2560)

  def testAppend(self):
    file1 = self.MakeFile('file1', 128)
    file2 = self.MakeFile('file2', 256)
    self.RunZip([self.zipname, file1, file2])
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 2)
    self.CloseZipFile()

    file3 = self.MakeFile('file3', 768)
    self.RunZip([self.zipname, file3])
    self.OpenZipFile()
    self.assertEqual(len(self.zipfile.namelist()), 3)


def main():
  suite = unittest.TestLoader().loadTestsFromTestCase(TestZip)
  suite.addTests(doctest.DocTestSuite(oshelpers))
  result = unittest.TextTestRunner(verbosity=2).run(suite)
  return int(not result.wasSuccessful())


if __name__ == '__main__':
  sys.exit(main())
