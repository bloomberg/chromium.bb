#!/usr/bin/python2.6
#
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for install_nsis.py."""

import os
import shutil
import subprocess
import sys
import unittest

from build_tools import install_nsis


class TestInstallNsis(unittest.TestCase):
  """This class tests basic functionality of the install_nsis package"""
  def setUp(self):
    self.nsis_installer_ = os.path.join(os.path.abspath('build_tools'),
                                        install_nsis.NSIS_INSTALLER)
    self.target_dir_ = os.path.join(os.path.dirname(self.nsis_installer_),
                                    'nsis_test',
                                    'NSIS')
  def tearDown(self):
    shutil.rmtree(os.path.dirname(self.target_dir_), ignore_errors=True)

  def testNsisInstallerExists(self):
    """Ensure that the correct version of NSIS is present."""
    self.assertTrue(os.path.exists(self.nsis_installer_))

  def testBogusNsisInstaller(self):
    """Make sure the installer handles invalid directory names."""
    self.assertRaises(IOError, install_nsis.InstallNsis, 'bogus', 'not_a_dir')

  def testNsisInstaller(self):
    """Make sure the installer produces an NSIS directory."""
    install_nsis.InstallNsis(self.nsis_installer_, self.target_dir_)
    self.assertTrue(os.path.exists(os.path.join(self.target_dir_,
                                                'makensis.exe')))

  def testAccessControlExtensions(self):
    """Make sure that the AccessControl extensions can be installed."""
    script_dir = os.path.dirname(self.nsis_installer_)
    install_nsis.InstallAccessControlExtensions(
        script_dir,
        os.path.join(script_dir, install_nsis.ACCESS_CONTROL_ZIP),
        self.target_dir_)
    self.assertTrue(os.path.exists(
        os.path.join(self.target_dir_, 'Plugins', 'AccessControl.dll')))
    self.assertTrue(os.path.exists(
        os.path.join(self.target_dir_, 'Plugins', 'AccessControlW.dll')))

  def testMkLinkExtensions(self):
    """Make sure the MkLink extensions are installed."""
    script_dir = os.path.dirname(self.nsis_installer_)
    install_nsis.InstallMkLinkExtensions(
        os.path.join(script_dir, install_nsis.MKLINK_DLL),
        self.target_dir_)
    self.assertTrue(os.path.exists(
        os.path.join(self.target_dir_, 'Plugins', 'MkLink.dll')))

  def testForceTargetInstall(self):
    """Test that a force install to a target directory works."""
    try:
      # Mock the NSIS install directories so that Install() thinks NSIS is
      # already installed.
      os.makedirs(os.path.join(self.target_dir_, 'Plugins'), mode=0777)
    except OSError:
      pass
    self.assertFalse(os.path.exists(os.path.join(self.target_dir_,
                                                 'makensis.exe')))
    self.assertFalse(os.path.exists(
        os.path.join(self.target_dir_, 'Plugins', 'AccessControl.dll')))
    self.assertFalse(os.path.exists(
        os.path.join(self.target_dir_, 'Plugins', 'AccessControlW.dll')))
    self.assertFalse(os.path.exists(
        os.path.join(self.target_dir_, 'Plugins', 'MkLink.dll')))

    install_nsis.Install(os.path.dirname(self.nsis_installer_),
                         target_dir=self.target_dir_,
                         force=True)

    self.assertTrue(os.path.exists(os.path.join(self.target_dir_,
                                                'makensis.exe')))
    self.assertTrue(os.path.exists(
        os.path.join(self.target_dir_, 'Plugins', 'AccessControl.dll')))
    self.assertTrue(os.path.exists(
        os.path.join(self.target_dir_, 'Plugins', 'AccessControlW.dll')))
    self.assertTrue(os.path.exists(
        os.path.join(self.target_dir_, 'Plugins', 'MkLink.dll')))


def RunTests():
  suite = unittest.TestLoader().loadTestsFromTestCase(TestInstallNsis)
  result = unittest.TextTestRunner(verbosity=2).run(suite)

  return int(not result.wasSuccessful())


if __name__ == '__main__':
  sys.exit(RunTests())
