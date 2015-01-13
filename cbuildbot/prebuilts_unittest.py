# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for prebuilts."""

from __future__ import print_function

import mock
import os

from chromite.cbuildbot import constants
from chromite.cbuildbot import prebuilts
from chromite.lib import cros_build_lib_unittest
from chromite.lib import osutils


# pylint: disable=W0212
class PrebuiltTest(cros_build_lib_unittest.RunCommandTempDirTestCase):
  """Test general cbuildbot command methods."""

  def setUp(self):
    self._board = 'test-board'
    self._buildroot = self.tempdir
    self._overlays = ['%s/src/third_party/chromiumos-overlay' % self._buildroot]
    self._chroot = os.path.join(self._buildroot, 'chroot')
    os.makedirs(os.path.join(self._buildroot, '.repo'))

  def testUploadPrebuilts(self, builder_type=constants.PFQ_TYPE, private=False,
                          chrome_rev=None, version=None):
    """Test UploadPrebuilts with a public location."""
    prebuilts.UploadPrebuilts(builder_type, chrome_rev, private,
                              buildroot=self._buildroot, board=self._board,
                              version=version)
    self.assertCommandContains([builder_type, 'gs://chromeos-prebuilt'])

  def testUploadPrivatePrebuilts(self):
    """Test UploadPrebuilts with a private location."""
    self.testUploadPrebuilts(private=True)

  def testChromePrebuilts(self):
    """Test UploadPrebuilts for Chrome prebuilts."""
    self.testUploadPrebuilts(builder_type=constants.CHROME_PFQ_TYPE,
                             chrome_rev='tot')

  def testSdkPrebuilts(self):
    """Test UploadPrebuilts for SDK builds."""
    # A magical date for a magical time.
    version = '1994.04.02.000000'

    # Fake out toolchain tarballs.
    tarball_dir = os.path.join(self._buildroot, constants.DEFAULT_CHROOT_DIR,
                               constants.SDK_TOOLCHAINS_OUTPUT)
    osutils.SafeMakedirs(tarball_dir)

    tarball_args = []
    for tarball_base in ('i686', 'arm-none-eabi'):
      tarball = '%s.tar.xz' % tarball_base
      tarball_path = os.path.join(tarball_dir, tarball)
      osutils.Touch(tarball_path)
      tarball_arg = '%s:%s' % (tarball_base, tarball_path)
      tarball_args.append(['--toolchain-tarball', tarball_arg])

    self.testUploadPrebuilts(builder_type=constants.CHROOT_BUILDER_TYPE,
                             version=version)
    self.assertCommandContains(['--toolchain-upload-path',
                                '1994/04/%%(target)s-%(version)s.tar.xz'])
    for args in tarball_args:
      self.assertCommandContains(args)
    self.assertCommandContains(['--set-version', version])
    self.assertCommandContains(['--prepackaged-tarball',
                                os.path.join(self._buildroot,
                                             'built-sdk.tar.xz')])

  def testDevInstallerPrebuilts(self, packages=('package1', 'package2')):
    """Test UploadDevInstallerPrebuilts."""
    args = ['gs://dontcare', 'some_path_to_key', 'https://my_test/location']
    with mock.patch.object(prebuilts, '_AddPackagesForPrebuilt',
                           return_value=packages):
      prebuilts.UploadDevInstallerPrebuilts(*args, buildroot=self._buildroot,
                                            board=self._board)
    self.assertCommandContains([constants.CANARY_TYPE] + args[2:] + args[0:2])

  def testAddPackagesForPrebuilt(self):
    """Test AddPackagesForPrebuilt."""
    self.assertEqual(prebuilts._AddPackagesForPrebuilt('/'), None)

    data = """# comment!
cat/pkg-0
ca-t2/pkg2-123
ca-t3/pk-g4-4.0.1-r333
"""
    pkgs = [
        'cat/pkg',
        'ca-t2/pkg2',
        'ca-t3/pk-g4',
    ]
    cmds = ['--packages=' + x for x in pkgs]
    f = os.path.join(self.tempdir, 'package.provided')
    osutils.WriteFile(f, data)
    self.assertEqual(prebuilts._AddPackagesForPrebuilt(f), cmds)

  def testMissingDevInstallerFile(self):
    """Test that we raise an exception when the installer file is missing."""
    self.assertRaises(prebuilts.PackageFileMissing,
                      self.testDevInstallerPrebuilts, packages=())
