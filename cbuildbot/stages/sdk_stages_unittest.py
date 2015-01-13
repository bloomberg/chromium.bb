# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for SDK stages."""

from __future__ import print_function

import json
import os

from chromite.cbuildbot import commands
from chromite.cbuildbot import constants
from chromite.cbuildbot.stages import sdk_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import perf_uploader
from chromite.lib import portage_util


class SDKBuildToolchainsStageTest(generic_stages_unittest.AbstractStageTest):
  """Tests SDK toolchain building."""

  def setUp(self):
    # This code has its own unit tests, so no need to go testing it here.
    self.run_mock = self.PatchObject(commands, 'RunBuildScript')

  def ConstructStage(self):
    return sdk_stages.SDKBuildToolchainsStage(self._run)

  def testNormal(self):
    """Basic run through the main code."""
    self._Prepare('chromiumos-sdk')
    self.RunStage()
    self.assertEqual(self.run_mock.call_count, 2)

    # Sanity check args passed to RunBuildScript.
    for call in self.run_mock.call_args_list:
      buildroot, cmd = call[0]
      self.assertTrue(isinstance(buildroot, basestring))
      self.assertTrue(isinstance(cmd, (tuple, list)))
      for ele in cmd:
        self.assertTrue(isinstance(ele, basestring))


class SDKPackageStageTest(generic_stages_unittest.AbstractStageTest):
  """Tests SDK package and Manifest creation."""

  fake_packages = (('cat1/package', '1'), ('cat1/package', '2'),
                   ('cat2/package', '3'), ('cat2/package', '4'))

  def setUp(self):
    # Replace SudoRunCommand, since we don't care about sudo.
    self.PatchObject(cros_build_lib, 'SudoRunCommand',
                     wraps=cros_build_lib.RunCommand)

    # Prepare a fake chroot.
    self.fake_chroot = os.path.join(self.build_root, 'chroot/build/amd64-host')
    self.fake_json_data = {}
    osutils.SafeMakedirs(self.fake_chroot)
    osutils.Touch(os.path.join(self.fake_chroot, 'file'))
    for package, v in self.fake_packages:
      cpv = portage_util.SplitCPV('%s-%s' % (package, v))
      key = '%s/%s' % (cpv.category, cpv.package)
      self.fake_json_data.setdefault(key, []).append([v, {}])

  def ConstructStage(self):
    return sdk_stages.SDKPackageStage(self._run)

  def testTarballCreation(self):
    """Tests whether we package the tarball and correctly create a Manifest."""
    # We'll test this separately.
    self.PatchObject(sdk_stages.SDKPackageStage, '_SendPerfValues')

    self._Prepare('chromiumos-sdk')
    fake_tarball = os.path.join(self.build_root, 'built-sdk.tar.xz')
    fake_manifest = os.path.join(self.build_root,
                                 'built-sdk.tar.xz.Manifest')

    self.PatchObject(portage_util, 'ListInstalledPackages',
                     return_value=self.fake_packages)

    self.RunStage()

    # Check tarball for the correct contents.
    output = cros_build_lib.RunCommand(
        ['tar', '-I', 'xz', '-tvf', fake_tarball],
        capture_output=True).output.splitlines()
    # First line is './', use it as an anchor, count the chars, and strip as
    # much from all other lines.
    stripchars = len(output[0]) - 1
    tar_lines = [x[stripchars:] for x in output]
    self.assertNotIn('/build/amd64-host/', tar_lines)
    self.assertIn('/file', tar_lines)
    # Verify manifest contents.
    real_json_data = json.loads(osutils.ReadFile(fake_manifest))
    self.assertEqual(real_json_data['packages'],
                     self.fake_json_data)

  def testPerf(self):
    """Check perf data points are generated/uploaded."""
    m = self.PatchObject(perf_uploader, 'UploadPerfValues')

    sdk_data = 'asldjfasf'
    sdk_size = len(sdk_data)
    sdk_tarball = os.path.join(self.tempdir, 'sdk.tar.xz')
    osutils.WriteFile(sdk_tarball, sdk_data)

    tarball_dir = os.path.join(self.tempdir, constants.DEFAULT_CHROOT_DIR,
                               constants.SDK_TOOLCHAINS_OUTPUT)
    arm_tar = os.path.join(tarball_dir, 'arm-cros-linux-gnu.tar.xz')
    x86_tar = os.path.join(tarball_dir, 'i686-pc-linux-gnu.tar.xz')
    osutils.Touch(arm_tar, makedirs=True)
    osutils.Touch(x86_tar, makedirs=True)

    # pylint: disable=protected-access
    sdk_stages.SDKPackageStage._SendPerfValues(
        self.tempdir, sdk_tarball, 'http://some/log', '123.4.5.6', 'sdk-bot')
    # pylint: enable=protected-access

    perf_values = m.call_args[0][0]
    exp = perf_uploader.PerformanceValue(
        description='base',
        value=sdk_size,
        units='bytes',
        higher_is_better=False,
        graph='cros-sdk-size',
        stdio_uri='http://some/log',
    )
    self.assertEqual(exp, perf_values[0])

    exp = set((
        perf_uploader.PerformanceValue(
            description='arm-cros-linux-gnu',
            value=0,
            units='bytes',
            higher_is_better=False,
            graph='cros-sdk-size',
            stdio_uri='http://some/log',
        ),
        perf_uploader.PerformanceValue(
            description='i686-pc-linux-gnu',
            value=0,
            units='bytes',
            higher_is_better=False,
            graph='cros-sdk-size',
            stdio_uri='http://some/log',
        ),
        perf_uploader.PerformanceValue(
            description='base_plus_arm-cros-linux-gnu',
            value=sdk_size,
            units='bytes',
            higher_is_better=False,
            graph='cros-sdk-size',
            stdio_uri='http://some/log',
        ),
        perf_uploader.PerformanceValue(
            description='base_plus_i686-pc-linux-gnu',
            value=sdk_size,
            units='bytes',
            higher_is_better=False,
            graph='cros-sdk-size',
            stdio_uri='http://some/log',
        ),
    ))
    self.assertEqual(exp, set(perf_values[1:]))

    platform_name = m.call_args[0][1]
    self.assertEqual(platform_name, 'sdk-bot')

    test_name = m.call_args[0][2]
    self.assertEqual(test_name, 'sdk')

    kwargs = m.call_args[1]
    self.assertEqual(kwargs['revision'], 123456)


class SDKTestStageTest(generic_stages_unittest.AbstractStageTest):
  """Tests SDK test phase."""

  def setUp(self):
    # This code has its own unit tests, so no need to go testing it here.
    self.run_mock = self.PatchObject(cros_build_lib, 'RunCommand')

  def ConstructStage(self):
    return sdk_stages.SDKTestStage(self._run)

  def testNormal(self):
    """Basic run through the main code."""
    self._Prepare('chromiumos-sdk')
    self.RunStage()
