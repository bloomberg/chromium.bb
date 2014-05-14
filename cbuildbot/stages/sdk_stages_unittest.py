#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for SDK stages."""

import mox
import json
import os
import sys

sys.path.insert(0, os.path.abspath('%s/../../..' % os.path.dirname(__file__)))
from chromite.cbuildbot import portage_utilities
from chromite.cbuildbot.stages import sdk_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils


# pylint: disable=R0901
class SDKStageTest(generic_stages_unittest.AbstractStageTest):
  """Tests SDK package and Manifest creation."""
  fake_packages = [('cat1/package', '1'), ('cat1/package', '2'),
                   ('cat2/package', '3'), ('cat2/package', '4')]
  fake_json_data = {}
  fake_chroot = None

  def setUp(self):
    # Replace SudoRunCommand, since we don't care about sudo.
    self._OriginalSudoRunCommand = cros_build_lib.SudoRunCommand
    cros_build_lib.SudoRunCommand = cros_build_lib.RunCommand

    # Prepare a fake chroot.
    self.fake_chroot = os.path.join(self.build_root, 'chroot/build/amd64-host')
    osutils.SafeMakedirs(self.fake_chroot)
    osutils.Touch(os.path.join(self.fake_chroot, 'file'))
    for package, v in self.fake_packages:
      cpv = portage_utilities.SplitCPV('%s-%s' % (package, v))
      key = '%s/%s' % (cpv.category, cpv.package)
      self.fake_json_data.setdefault(key, []).append([v, {}])

  def tearDown(self):
    cros_build_lib.SudoRunCommand = self._OriginalSudoRunCommand

  def ConstructStage(self):
    return sdk_stages.SDKPackageStage(self._run)

  def testTarballCreation(self):
    """Tests whether we package the tarball and correctly create a Manifest."""
    self._Prepare('chromiumos-sdk')
    fake_tarball = os.path.join(self.build_root, 'built-sdk.tar.xz')
    fake_manifest = os.path.join(self.build_root,
                                 'built-sdk.tar.xz.Manifest')
    self.mox.StubOutWithMock(portage_utilities, 'ListInstalledPackages')
    self.mox.StubOutWithMock(sdk_stages.SDKPackageStage,
                             'CreateRedistributableToolchains')

    portage_utilities.ListInstalledPackages(self.fake_chroot).AndReturn(
        self.fake_packages)
    # This code has its own unit tests, so no need to go testing it here.
    # pylint: disable=E1120
    sdk_stages.SDKPackageStage.CreateRedistributableToolchains(mox.IgnoreArg())

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

    # Check tarball for the correct contents.
    output = cros_build_lib.RunCommand(
        ['tar', '-I', 'xz', '-tvf', fake_tarball],
        capture_output=True).output.splitlines()
    # First line is './', use it as an anchor, count the chars, and strip as
    # much from all other lines.
    stripchars = len(output[0]) - 1
    tar_lines = [x[stripchars:] for x in output]
    # TODO(ferringb): replace with assertIn.
    self.assertFalse('/build/amd64-host/' in tar_lines)
    self.assertTrue('/file' in tar_lines)
    # Verify manifest contents.
    real_json_data = json.loads(osutils.ReadFile(fake_manifest))
    self.assertEqual(real_json_data['packages'],
                     self.fake_json_data)


if __name__ == '__main__':
  cros_test_lib.main()
