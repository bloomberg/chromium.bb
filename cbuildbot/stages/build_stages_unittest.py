# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for build stages."""

from __future__ import print_function

import contextlib
import os

from chromite.cbuildbot import cbuildbot_unittest
from chromite.cbuildbot import chromeos_config
from chromite.cbuildbot import commands
from chromite.cbuildbot import config_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot.stages import build_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import parallel
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock
from chromite.lib import path_util
from chromite.lib import portage_util

from chromite.cbuildbot.stages.generic_stages_unittest import patch
from chromite.cbuildbot.stages.generic_stages_unittest import patches


# pylint: disable=too-many-ancestors


class InitSDKTest(generic_stages_unittest.RunCommandAbstractStageTestCase):
  """Test building the SDK"""

  # pylint: disable=protected-access

  def setUp(self):
    self.PatchObject(cros_build_lib, 'GetChrootVersion', return_value='12')
    self.cros_sdk = os.path.join(self.tempdir, 'buildroot',
                                 constants.CHROMITE_BIN_SUBDIR, 'cros_sdk')

  def ConstructStage(self):
    return build_stages.InitSDKStage(self._run)

  def testFullBuildWithExistingChroot(self):
    """Tests whether we create chroots for full builds."""
    self._PrepareFull()
    self._Run(dir_exists=True)
    self.assertCommandContains([self.cros_sdk])

  def testBinBuildWithMissingChroot(self):
    """Tests whether we create chroots when needed."""
    self._PrepareBin()
    # Do not force chroot replacement in build config.
    self._run._config.chroot_replace = False
    self._Run(dir_exists=False)
    self.assertCommandContains([self.cros_sdk])

  def testFullBuildWithMissingChroot(self):
    """Tests whether we create chroots when needed."""
    self._PrepareFull()
    self._Run(dir_exists=True)
    self.assertCommandContains([self.cros_sdk])

  def testFullBuildWithNoSDK(self):
    """Tests whether the --nosdk option works."""
    self._PrepareFull(extra_cmd_args=['--nosdk'])
    self._Run(dir_exists=False)
    self.assertCommandContains([self.cros_sdk, '--bootstrap'])

  def testBinBuildWithExistingChroot(self):
    """Tests whether the --nosdk option works."""
    self._PrepareFull(extra_cmd_args=['--nosdk'])
    # Do not force chroot replacement in build config.
    self._run._config.chroot_replace = False
    self._run._config.separate_debug_symbols = False
    self._run.config.useflags = ['foo']
    self._Run(dir_exists=True)
    self.assertCommandContains([self.cros_sdk], expected=False)
    self.assertCommandContains(['./run_chroot_version_hooks'],
                               enter_chroot=True, extra_env={'USE': 'foo'})


class SetupBoardTest(generic_stages_unittest.RunCommandAbstractStageTestCase):
  """Test building the board"""

  def ConstructStage(self):
    return build_stages.SetupBoardStage(self._run, self._current_board)

  def _RunFull(self, dir_exists=False):
    """Helper for testing a full builder."""
    self._Run(dir_exists)
    self.assertCommandContains(['./update_chroot'])
    cmd = ['./setup_board', '--board=%s' % self._current_board, '--nousepkg']
    self.assertCommandContains(cmd)
    cmd = ['./setup_board', '--skip_chroot_upgrade']
    self.assertCommandContains(cmd)

  def testFullBuildWithProfile(self):
    """Tests whether full builds add profile flag when requested."""
    self._PrepareFull(extra_config={'profile': 'foo'})
    self._RunFull(dir_exists=False)
    self.assertCommandContains(['./setup_board', '--profile=foo'])

  def testFullBuildWithOverriddenProfile(self):
    """Tests whether full builds add overridden profile flag when requested."""
    self._PrepareFull(extra_cmd_args=['--profile', 'smock'])
    self._RunFull(dir_exists=False)
    self.assertCommandContains(['./setup_board', '--profile=smock'])

  def _RunBin(self, dir_exists):
    """Helper for testing a binary builder."""
    self._Run(dir_exists)
    update_nousepkg = (not self._run.config.usepkg_toolchain or
                       self._run.options.latest_toolchain)
    self.assertCommandContains(['./update_chroot', '--nousepkg'],
                               expected=update_nousepkg)
    self.assertCommandContains(['./setup_board'])
    cmd = ['./setup_board', '--skip_chroot_upgrade']
    self.assertCommandContains(cmd)
    cmd = ['./setup_board', '--nousepkg']
    self.assertCommandContains(
        cmd, not self._run.config.usepkg_build_packages)

  def testBinBuildWithLatestToolchain(self):
    """Tests whether we use --nousepkg for creating the board."""
    self._PrepareBin()
    self._run.options.latest_toolchain = True
    self._RunBin(dir_exists=False)

  def testBinBuildWithLatestToolchainAndDirExists(self):
    """Tests whether we use --nousepkg for creating the board."""
    self._PrepareBin()
    self._run.options.latest_toolchain = True
    self._RunBin(dir_exists=True)

  def testBinBuildWithNoToolchainPackages(self):
    """Tests whether we use --nousepkg for creating the board."""
    self._PrepareBin()
    self._run.config.usepkg_toolchain = False
    self._RunBin(dir_exists=False)

  def testSDKBuild(self):
    """Tests whether we use --skip_chroot_upgrade for SDK builds."""
    extra_config = {'build_type': constants.CHROOT_BUILDER_TYPE}
    self._PrepareFull(extra_config=extra_config)
    self._Run(dir_exists=False)
    self.assertCommandContains(['./update_chroot'], expected=False)
    self.assertCommandContains(['./setup_board', '--skip_chroot_upgrade'])


class UprevStageTest(generic_stages_unittest.AbstractStageTestCase):
  """Tests for the UprevStage class."""

  def setUp(self):
    self.uprev_mock = self.PatchObject(commands, 'UprevPackages')

    self._Prepare()

  def ConstructStage(self):
    return build_stages.UprevStage(self._run)

  def testBuildRev(self):
    """Uprevving the build without uprevving chrome."""
    self._run.config['uprev'] = True
    self.RunStage()
    self.assertTrue(self.uprev_mock.called)

  def testNoRev(self):
    """No paths are enabled."""
    self._run.config['uprev'] = False
    self.RunStage()
    self.assertFalse(self.uprev_mock.called)


class AllConfigsTestCase(generic_stages_unittest.AbstractStageTestCase,
                         cros_test_lib.OutputTestCase):
  """Test case for testing against all bot configs."""

  def ConstructStage(self):
    """Bypass lint warning"""
    generic_stages_unittest.AbstractStageTestCase.ConstructStage(self)

  @contextlib.contextmanager
  def RunStageWithConfig(self, mock_configurator=None):
    """Run the given config"""
    try:
      with cros_build_lib_unittest.RunCommandMock() as rc:
        rc.SetDefaultCmdResult()
        if mock_configurator:
          mock_configurator(rc)
        with self.OutputCapturer():
          with cros_test_lib.LoggingCapturer():
            self.RunStage()

        yield rc

    except AssertionError as ex:
      msg = '%s failed the following test:\n%s' % (self._bot_id, ex)
      raise AssertionError(msg)

  def RunAllConfigs(self, task, skip_missing=False, site_config=None):
    """Run |task| against all major configurations"""
    if site_config is None:
      site_config = chromeos_config.GetConfig()

    with parallel.BackgroundTaskRunner(task) as queue:
      # Loop through all major configuration types and pick one from each.
      for bot_type in config_lib.CONFIG_TYPE_DUMP_ORDER:
        for bot_id in site_config:
          if bot_id.endswith(bot_type):
            # Skip any config without a board, since those configs do not
            # build packages.
            cfg = site_config[bot_id]
            if cfg.boards:
              # Skip boards w/out a local overlay.  Like when running a
              # public manifest and testing private-only boards.
              if skip_missing:
                try:
                  for b in cfg.boards:
                    portage_util.FindPrimaryOverlay(constants.BOTH_OVERLAYS, b)
                except portage_util.MissingOverlayException:
                  continue

              queue.put([bot_id])
              break


class BuildPackagesStageTest(AllConfigsTestCase,
                             cbuildbot_unittest.SimpleBuilderTestCase):
  """Tests BuildPackagesStage."""

  def setUp(self):
    self._release_tag = None
    self.PatchObject(commands, 'ExtractDependencies', return_value=dict())

  def ConstructStage(self):
    self._run.attrs.release_tag = self._release_tag
    return build_stages.BuildPackagesStage(self._run, self._current_board)

  def RunTestsWithBotId(self, bot_id, options_tests=True):
    """Test with the config for the specified bot_id."""
    self._Prepare(bot_id)
    self._run.options.tests = options_tests

    with self.RunStageWithConfig() as rc:
      cfg = self._run.config
      rc.assertCommandContains(['./build_packages'])
      rc.assertCommandContains(['./build_packages', '--skip_chroot_upgrade'])
      rc.assertCommandContains(['./build_packages', '--nousepkg'],
                               expected=not cfg['usepkg_build_packages'])
      rc.assertCommandContains(['./build_packages', '--nowithautotest'],
                               expected=not self._run.options.tests)

  def testAllConfigs(self):
    """Test all major configurations"""
    self.RunAllConfigs(self.RunTestsWithBotId)

  def testNoTests(self):
    """Test that self.options.tests = False works."""
    self.RunTestsWithBotId('x86-generic-paladin', options_tests=False)

  def testIgnoreExtractDependenciesError(self):
    """Igore errors when failing to extract dependencies."""
    self.PatchObject(commands, 'ExtractDependencies',
                     side_effect=Exception('unmet dependency'))
    self.RunTestsWithBotId('x86-generic-paladin')


class BuildImageStageMock(partial_mock.PartialMock):
  """Partial mock for BuildImageStage."""

  TARGET = 'chromite.cbuildbot.stages.build_stages.BuildImageStage'
  ATTRS = ('_BuildImages', '_GenerateAuZip')

  def _BuildImages(self, *args, **kwargs):
    with patches(
        patch(os, 'symlink'),
        patch(os, 'readlink', return_value='foo.txt')):
      self.backup['_BuildImages'](*args, **kwargs)

  def _GenerateAuZip(self, *args, **kwargs):
    with patch(path_util, 'ToChrootPath',
               return_value='/chroot/path'):
      self.backup['_GenerateAuZip'](*args, **kwargs)


class BuildImageStageTest(BuildPackagesStageTest):
  """Tests BuildImageStage."""

  def setUp(self):
    self.StartPatcher(BuildImageStageMock())

  def ConstructStage(self):
    return build_stages.BuildImageStage(self._run, self._current_board)

  def RunTestsWithReleaseConfig(self, release_tag):
    self._release_tag = release_tag

    with parallel_unittest.ParallelMock():
      with self.RunStageWithConfig() as rc:
        cfg = self._run.config
        cmd = ['./build_image', '--version=%s' % (self._release_tag or '')]
        rc.assertCommandContains(cmd, expected=cfg['images'])
        rc.assertCommandContains(['./image_to_vm.sh'],
                                 expected=cfg['vm_tests'])
        cmd = ['./build_library/generate_au_zip.py', '-o', '/chroot/path']
        rc.assertCommandContains(cmd, expected=cfg['images'])

  def RunTestsWithBotId(self, bot_id, options_tests=True):
    """Test with the config for the specified bot_id."""
    release_tag = '0.0.1'
    self._Prepare(bot_id)
    self._run.options.tests = options_tests
    self._run.attrs.release_tag = release_tag

    task = self.RunTestsWithReleaseConfig
    # TODO: This test is broken atm with tag=None.
    steps = [lambda tag=x: task(tag) for x in (release_tag,)]
    parallel.RunParallelSteps(steps)
