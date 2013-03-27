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
import mock

# For getos, the module under test
sys.path.append(TOOLS_DIR)
import getos
import oshelpers


class TestCaseExtended(unittest.TestCase):
  """Monkey patch some 2.7-only TestCase features."""
  # TODO(sbc): remove this once we switch to python2.7 everywhere
  def assertIn(self, expr1, expr2, msg=None):
    if hasattr(super(TestCaseExtended, self), 'assertIn'):
      return super(TestCaseExtended, self).assertIn(expr1, expr2, msg)
    if expr1 not in expr2:
      self.fail(msg or '%r not in %r' % (expr1, expr2))


class TestGetos(TestCaseExtended):
  def setUp(self):
    self.patch1 = mock.patch.dict('os.environ',
                                  {'NACL_SDK_ROOT': os.path.dirname(TOOLS_DIR)})
    self.patch1.start()
    self.patch2 = mock.patch.object(oshelpers, 'FindExeInPath',
                                    return_value='/bin/ls')
    self.patch2.start()

  def tearDown(self):
    self.patch1.stop()
    self.patch2.stop()

  def testGetSDKPath(self):
    """honors environment variable."""
    with mock.patch.dict('os.environ', {'NACL_SDK_ROOT': 'dummy'}):
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
    """returns a valid architecture."""
    arch = getos.GetSystemArch(getos.GetPlatform())
    self.assertIn(arch, ('x86_64', 'x86_32', 'arm'))

  def testGetChromePathEnv(self):
    """honors CHROME_PATH environment."""
    with mock.patch.dict('os.environ', {'CHROME_PATH': '/dummy/file'}):
      expect = "Invalid CHROME_PATH.*/dummy/file"
      if hasattr(self, 'assertRaisesRegexp'):
        with self.assertRaisesRegexp(getos.Error, expect):
          getos.GetChromePath()
      else:
        # TODO(sbc): remove this path once we switch to python2.7 everywhere
        self.assertRaises(getos.Error, getos.GetChromePath)

  def testGetChromePathCheckExists(self):
    """checks that existence of explicitly CHROME_PATH is checked."""
    mock_location = '/bin/ls'
    if getos.GetPlatform() == 'win':
      mock_location = 'c:\\nowhere'
    with mock.patch.dict('os.environ', {'CHROME_PATH': mock_location}):
      with mock.patch('os.path.exists') as mock_exists:
        chrome = getos.GetChromePath()
        self.assertEqual(chrome, mock_location)
        mock_exists.assert_called_with(chrome)

  def testGetHelperPath(self):
    """GetHelperPath checks that helper exists."""
    platform = getos.GetPlatform()
    # The helper is only needed on linux. On other platforms
    # GetHelperPath() simply return empty string.
    if platform == 'linux':
      with mock.patch('os.path.exists') as mock_exists:
        helper = getos.GetHelperPath(platform)
        mock_exists.assert_called_with(helper)
    else:
      helper = getos.GetHelperPath(platform)
      self.assertEqual(helper, '')

  def testGetIrtBinPath(self):
    """GetIrtBinPath checks that irtbin exists."""
    platform = getos.GetPlatform()
    with mock.patch('os.path.exists') as mock_exists:
      irt = getos.GetIrtBinPath(platform)
      mock_exists.assert_called_with(irt)

  def testGetLoaderPath(self):
    """checks that loader exists."""
    platform = getos.GetPlatform()
    with mock.patch('os.path.exists') as mock_exists:
      loader = getos.GetLoaderPath(platform)
      mock_exists.assert_called_with(loader)

  def testGetNaClArch(self):
    """returns a valid architecture."""
    platform = getos.GetPlatform()
    # Since the unix implementation of GetNaClArch will run objdump on the
    # chrome binary, and we want to be able to run this test without chrome
    # installed we mock the GetChromePath call to return a known system binary,
    # which objdump will work with.
    with mock.patch('getos.GetChromePath') as mock_chrome_path:
      mock_chrome_path.return_value = '/bin/ls'
      arch = getos.GetNaClArch(platform)
      self.assertIn(arch, ('x86_64', 'x86_32', 'arm'))


class TestGetosWithTempdir(TestCaseExtended):
  def setUp(self):
    self.tempdir = tempfile.mkdtemp("_sdktest")
    self.patch = mock.patch.dict('os.environ',
                                  {'NACL_SDK_ROOT': self.tempdir})
    self.patch.start()

  def tearDown(self):
    shutil.rmtree(self.tempdir)
    self.patch.stop()

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
