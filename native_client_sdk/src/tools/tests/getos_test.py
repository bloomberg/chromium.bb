#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import sys
import tempfile
import unittest

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
TOOLS_DIR = os.path.dirname(SCRIPT_DIR)
CHROME_SRC = os.path.dirname(os.path.dirname(os.path.dirname(TOOLS_DIR)))
MOCK_DIR = os.path.join(CHROME_SRC, "third_party", "pymock")

# For the mock library
sys.path.append(MOCK_DIR)
from mock import patch

# For getos, the module under test
sys.path.append(TOOLS_DIR)
import getos


class TestCaseExtended(unittest.TestCase):
  """Monkey patch some 2.7-only TestCase features."""
  def assertIn(self, expr1, expr2, msg=None):
    if hasattr(super(TestCaseExtended, self), 'assertIn'):
      return super(TestCaseExtended, self).assertIn(expr1, expr2, msg)
    if expr1 not in expr2:
      self.fail(msg or '%r not in %r' % (expr1, expr2))


class TestGetos(TestCaseExtended):
  def setUp(self):
    self.sdkroot = os.environ.get('NACL_SDK_ROOT')
    os.environ['NACL_SDK_ROOT'] = os.path.dirname(TOOLS_DIR)

  def tearDown(self):
    if self.sdkroot != None:
      os.environ['NACL_SDK_ROOT'] = self.sdkroot

  def testGetSDKPath(self):
    """honors enironment variable."""
    with patch.dict('os.environ', {'NACL_SDK_ROOT': 'dummy'}):
      self.assertEqual(getos.GetSDKPath(), 'dummy')

  def testGetSDKPathDefault(self):
    """defaults to relative path."""
    del os.environ['NACL_SDK_ROOT']
    self.assertEqual(getos.GetSDKPath(), os.path.dirname(TOOLS_DIR))

  def testGetPlatform(self):
    """returns a valid platform."""
    platform = getos.GetPlatform()
    self.assertIn(platform, ('mac', 'linux', 'win'))

  def testGetSystemArch(self):
    """returns a valid architecuture."""
    arch = getos.GetSystemArch(getos.GetPlatform())
    self.assertIn(arch, ('x86_64', 'x86_32', 'arm'))

  def testGetChromePathEnv(self):
    """honors CHROME_PATH environment."""
    with patch.dict('os.environ', {'CHROME_PATH': '/dummy/file'}):
      expect = "Invalid CHROME_PATH.*/dummy/file"
      with self.assertRaisesRegexp(getos.Error, expect):
        getos.GetChromePath()

  def testGetChromePathCheckExists(self):
    """checks that chrome exists."""
    with patch.dict('os.environ', {'CHROME_PATH': '/bin/ls'}):
      with patch('os.path.exists') as mock_exists:
        chrome = getos.GetChromePath()
        mock_exists.assert_called_with(chrome)

  def testGetHelperPath(self):
    """checks that helper exists."""
    platform = getos.GetPlatform()
    with patch('os.path.exists') as mock_exists:
      helper = getos.GetHelperPath(platform)
      mock_exists.assert_called_with(helper)

  def testGetIrtBinPath(self):
    """checks that helper exists."""
    platform = getos.GetPlatform()
    with patch('os.path.exists') as mock_exists:
      irt = getos.GetIrtBinPath(platform)
      mock_exists.assert_called_with(irt)

  def testGetLoaderPath(self):
    """checks that loader exists."""
    platform = getos.GetPlatform()
    with patch('os.path.exists') as mock_exists:
      irt = getos.GetIrtBinPath(platform)
      mock_exists.assert_called_with(irt)

  def testGetChromeArch(self):
    """returns a valid architecuture."""
    platform = getos.GetPlatform()
    # Since the unix implementation of GetChromeArch will run objdump on the
    # chrome binary, and we want to be able to run this test without chrome
    # installed we mock the GetChromePath call to return a known system binary,
    # which objdump will work with.
    with patch('getos.GetChromePath') as mock_chrome_path:
      mock_chrome_path.return_value = '/bin/ls'
      arch = getos.GetChromeArch(platform)
      self.assertIn(arch, ('x86_64', 'x86_32', 'arm'))


class TestGetosWithTempdir(TestGetos):
  def setUp(self):
    self.old_sdkroot = os.environ.get('NACL_SDK_ROOT')
    self.tempdir = tempfile.mkdtemp("_sdktest")
    os.environ['NACL_SDK_ROOT'] = self.tempdir

  def tearDown(self):
    shutil.rmtree(self.tempdir)
    os.environ['NACL_SDK_ROOT'] = self.old_sdkroot

  def testGetSDKVersion(self):
    """correctly parses README to find SDK version."""
    expected_version = (16, 196)
    with open(os.path.join(self.tempdir, 'README'), 'w') as out:
      out.write('Version: %s\n' % expected_version[0])
      out.write('Revision: %s\n' % expected_version[1])

    version = getos.GetSDKVersion()
    self.assertEqual(version, expected_version)


if __name__ == '__main__':
  unittest.main()
