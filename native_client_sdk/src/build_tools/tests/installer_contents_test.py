#!/usr/bin/env python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for installer_contents.py."""

import os
import sys
import unittest

from build_tools import installer_contents


class TestInstallerContents(unittest.TestCase):
  """This class tests basic functionality of the installer_contents package"""
  def setUp(self):
    self.mock_path_list = ['file',
                           'file/in/a/path',
                           'dir/spec/',
                           '',
                           '/abs/path',
                          ]


  def testConvertToOSPaths(self):
    output = installer_contents.ConvertToOSPaths(self.mock_path_list)
    self.assertEqual(len(output), len(self.mock_path_list))
    self.assertEqual(output[0], 'file')
    self.assertEqual(output[1], os.path.join('file', 'in', 'a', 'path'))
    self.assertEqual(output[2], os.path.join('dir', 'spec', ''))
    self.assertEqual(output[3], '')
    self.assertEqual(output[4], os.path.join('abs', 'path'))

  def testGetDirectoriesFromPathList(self):
    output = installer_contents.GetDirectoriesFromPathList(self.mock_path_list)
    self.assertEqual(1, len(output))
    self.assertEqual(output[0], os.path.join('dir', 'spec', ''))

  def testGetFilesFromPathList(self):
    output = installer_contents.GetFilesFromPathList(self.mock_path_list)
    self.assertEqual(4, len(output))
    self.assertEqual(output[0], 'file')
    self.assertEqual(output[1], os.path.join('file', 'in', 'a', 'path'))
    self.assertEqual(output[2], '')
    self.assertEqual(output[3], os.path.join('abs', 'path'))

  def testGetToolchainManifest(self):
    self.assertRaises(KeyError,
                      installer_contents.GetToolchainManifest,
                      'notatoolchain')
    newlib_manifest_path = installer_contents.GetToolchainManifest('newlib')
    self.assertTrue(os.path.exists(newlib_manifest_path))
    glibc_manifest_path = installer_contents.GetToolchainManifest('glibc')
    self.assertTrue(os.path.exists(glibc_manifest_path))


def RunTests():
  suite = unittest.TestLoader().loadTestsFromTestCase(TestInstallerContents)
  result = unittest.TextTestRunner(verbosity=2).run(suite)

  return int(not result.wasSuccessful())


if __name__ == '__main__':
  sys.exit(RunTests())
