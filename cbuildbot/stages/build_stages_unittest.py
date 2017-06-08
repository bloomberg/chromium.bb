# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for build stages."""

from __future__ import print_function

import contextlib
import os
import tempfile

from chromite.cbuildbot import cbuildbot_unittest
from chromite.cbuildbot import chromeos_config
from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import build_stages
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.lib import auth
from chromite.lib import buildbucket_lib
from chromite.lib import cidb
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import fake_cidb
from chromite.lib import osutils
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
    self._update_metadata = False
    self._mock_configurator = None
    self.PatchObject(commands, 'ExtractDependencies', return_value=dict())

  def ConstructStage(self):
    self._run.attrs.release_tag = self._release_tag
    return build_stages.BuildPackagesStage(
        self._run, self._current_board,
        update_metadata=self._update_metadata)

  def RunTestsWithBotId(self, bot_id, options_tests=True):
    """Test with the config for the specified bot_id."""
    self._Prepare(bot_id)
    self._run.options.tests = options_tests

    with self.RunStageWithConfig(self._mock_configurator) as rc:
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
    """Ignore errors when failing to extract dependencies."""
    self.PatchObject(commands, 'ExtractDependencies',
                     side_effect=Exception('unmet dependency'))
    self.RunTestsWithBotId('x86-generic-paladin')

  def testFirmwareVersionsMixedImage(self):
    """Test that firmware versions are extracted correctly."""
    expected_main_firmware_version = 'reef_v1.1.5822-78709a5'
    expected_ec_firmware_version = 'Google_Reef.9042.30.0'

    def _HookRunCommandFirmwareUpdate(rc):
      # A mixed RO+RW image will have separate "(RW) version" fields.
      rc.AddCmdResult(partial_mock.ListRegex('chromeos-firmwareupdate'),
                      output='BIOS (RW) version: %s\nEC (RW) version: %s' %
                      (expected_main_firmware_version,
                       expected_ec_firmware_version))

    self._update_metadata = True
    update = os.path.join(
        self.build_root,
        'chroot/build/x86-generic/usr/sbin/chromeos-firmwareupdate')
    osutils.Touch(update, makedirs=True)

    self._mock_configurator = _HookRunCommandFirmwareUpdate
    self.RunTestsWithBotId('x86-generic-paladin', options_tests=False)
    board_metadata = (self._run.attrs.metadata.GetDict()['board-metadata']
                      .get('x86-generic'))
    if board_metadata:
      self.assertIn('main-firmware-version', board_metadata)
      self.assertEqual(board_metadata['main-firmware-version'],
                       expected_main_firmware_version)
      self.assertIn('ec-firmware-version', board_metadata)
      self.assertEqual(board_metadata['ec-firmware-version'],
                       expected_ec_firmware_version)
      self.assertFalse(self._run.attrs.metadata.GetDict()['unibuild'])

  def testFirmwareVersions(self):
    """Test that firmware versions are extracted correctly."""
    expected_main_firmware_version = 'reef_v1.1.5822-78709a5'
    expected_ec_firmware_version = 'Google_Reef.9042.30.0'

    def _HookRunCommandFirmwareUpdate(rc):
      rc.AddCmdResult(partial_mock.ListRegex('chromeos-firmwareupdate'),
                      output='BIOS version: %s\nEC version: %s' %
                      (expected_main_firmware_version,
                       expected_ec_firmware_version))

    self._update_metadata = True
    update = os.path.join(
        self.build_root,
        'chroot/build/x86-generic/usr/sbin/chromeos-firmwareupdate')
    osutils.Touch(update, makedirs=True)

    self._mock_configurator = _HookRunCommandFirmwareUpdate
    self.RunTestsWithBotId('x86-generic-paladin', options_tests=False)
    board_metadata = (self._run.attrs.metadata.GetDict()['board-metadata']
                      .get('x86-generic'))
    if board_metadata:
      self.assertIn('main-firmware-version', board_metadata)
      self.assertEqual(board_metadata['main-firmware-version'],
                       expected_main_firmware_version)
      self.assertIn('ec-firmware-version', board_metadata)
      self.assertEqual(board_metadata['ec-firmware-version'],
                       expected_ec_firmware_version)
      self.assertFalse(self._run.attrs.metadata.GetDict()['unibuild'])

  def testUnifiedBuilds(self):
    """Test that unified builds are marked as such."""
    def _HookRunCommandFdtget(rc):
      rc.AddCmdResult(partial_mock.ListRegex('fdtget'), output='reef')

    self._update_metadata = True
    fdtget = os.path.join(self.build_root,
                          'chroot/build/x86-generic/usr/bin/fdtget')
    osutils.Touch(fdtget, makedirs=True)
    self._mock_configurator = _HookRunCommandFdtget
    self.RunTestsWithBotId('x86-generic-paladin', options_tests=False)
    self.assertTrue(self._run.attrs.metadata.GetDict()['unibuild'])

  def testGoma(self):
    self.PatchObject(build_stages.BuildPackagesStage,
                     '_ShouldEnableGoma', return_value=True)
    self._Prepare('x86-generic-paladin')
    # Set dummy dir name to enable goma.
    with osutils.TempDir() as goma_dir, \
         tempfile.NamedTemporaryFile() as temp_goma_client_json:
      self._run.options.goma_dir = goma_dir
      self._run.options.goma_client_json = temp_goma_client_json.name

      stage = self.ConstructStage()
      # pylint: disable=protected-access
      chroot_args = stage._SetupGomaIfNecessary()
      self.assertEqual(
          ['--goma_dir', goma_dir,
           '--goma_client_json', temp_goma_client_json.name],
          chroot_args)
      # pylint: disable=protected-access
      portage_env = stage._portage_extra_env
      self.assertRegexpMatches(
          portage_env.get('GOMA_DIR', ''), '^/home/.*/goma$')
      self.assertIn(portage_env.get('USE', ''), 'goma')
      self.assertEqual(
          '/creds/service_accounts/service-account-goma-client.json',
          portage_env.get('GOMA_SERVICE_ACCOUNT_JSON_FILE', ''))

  def testGomaWithMissingCertFile(self):
    self.PatchObject(build_stages.BuildPackagesStage,
                     '_ShouldEnableGoma', return_value=True)
    self._Prepare('x86-generic-paladin')
    # Set dummy dir name to enable goma.
    with osutils.TempDir() as goma_dir:
      self._run.options.goma_dir = goma_dir
      self._run.options.goma_client_json = 'dummy-goma-client-json-path'

      stage = self.ConstructStage()
      with self.assertRaisesRegexp(ValueError, 'json file is missing'):
        # pylint: disable=protected-access
        stage._SetupGomaIfNecessary()

  def testGomaOnBotWithoutCertFile(self):
    self.PatchObject(build_stages.BuildPackagesStage,
                     '_ShouldEnableGoma', return_value=True)
    self.PatchObject(cros_build_lib, 'HostIsCIBuilder', return_value=True)
    self._Prepare('x86-generic-paladin')
    # Set dummy dir name to enable goma.
    with osutils.TempDir() as goma_dir:
      self._run.options.goma_dir = goma_dir
      stage = self.ConstructStage()

      with self.assertRaisesRegexp(
          ValueError, 'goma_client_json is not provided'):
        # pylint: disable=protected-access
        stage._SetupGomaIfNecessary()


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

  def testUnifiedBuilds(self):
    pass

class CleanUpStageTest(generic_stages_unittest.StageTestCase):
  """Test CleanUpStage."""

  # pylint: disable=protected-access

  BOT_ID = 'master-paladin'

  def setUp(self):
    self.PatchObject(buildbucket_lib, 'GetServiceAccount',
                     return_value=True)
    self.PatchObject(auth.AuthorizedHttp, '__init__',
                     return_value=None)
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     '_GetHost',
                     return_value=buildbucket_lib.BUILDBUCKET_TEST_HOST)

    self.fake_db = fake_cidb.FakeCIDBConnection()
    cidb.CIDBConnectionFactory.SetupMockCidb(self.fake_db)

    self.fake_db.InsertBuild(
        'test_builder', constants.WATERFALL_TRYBOT, 666, 'test_config',
        'test_hostname',
        status=constants.BUILDER_STATUS_INFLIGHT,
        timeout_seconds=23456,
        buildbucket_id='100')

    self.fake_db.InsertBuild(
        'test_builder', constants.WATERFALL_TRYBOT, 666, 'test_config',
        'test_hostname',
        status=constants.BUILDER_STATUS_INFLIGHT,
        timeout_seconds=23456,
        buildbucket_id='200')

    self._Prepare()

  def ConstructStage(self):
    return build_stages.CleanUpStage(self._run)

  def test_GetBuildbucketBucketsForSlavesOnMixedWaterfalls(self):
    """Test _GetBuildbucketBucketsForSlaves with mixed waterfalls."""
    stage = self.ConstructStage()
    slave_config_map = {
        'slave_1': config_lib.BuildConfig(
            name='slave1',
            active_waterfall=constants.WATERFALL_EXTERNAL),
        'slave_2': config_lib.BuildConfig(
            name='slave2',
            active_waterfall=constants.WATERFALL_INTERNAL),
        'slave_3': config_lib.BuildConfig(
            name='slave3',
            active_waterfall=None)
    }
    self.PatchObject(generic_stages.BuilderStage, '_GetSlaveConfigMap',
                     return_value=slave_config_map)
    buckets = stage._GetBuildbucketBucketsForSlaves()

    expected_list = ['master.chromiumos', 'master.chromeos']
    self.assertItemsEqual(expected_list, buckets)

  def test_GetBuildbucketBucketsForSlavesOnSingleWaterfall(self):
    """Test _GetBuildbucketBucketsForSlaves with a signle waterfall."""
    stage = self.ConstructStage()

    slave_config_map = {
        'slave_1': config_lib.BuildConfig(
            name='slave1',
            active_waterfall=constants.WATERFALL_INTERNAL),
        'slave_2': config_lib.BuildConfig(
            name='slave2',
            active_waterfall=constants.WATERFALL_INTERNAL)
    }
    self.PatchObject(generic_stages.BuilderStage, '_GetSlaveConfigMap',
                     return_value=slave_config_map)
    buckets = stage._GetBuildbucketBucketsForSlaves()

    expected_list = ['master.chromeos']
    self.assertItemsEqual(expected_list, buckets)

  def testCancelObsoleteSlaveBuilds(self):
    """Test CancelObsoleteSlaveBuilds."""
    buildbucket_id_1 = '100'
    buildbucket_id_2 = '200'

    searched_builds = [{
        'status': 'STARTED',
        'id': buildbucket_id_1,
        'tags':[
            'bot_id:build265-m2',
            'build_type:tryjob',
            'master:False']
    }, {
        'status': 'STARTED',
        'id': buildbucket_id_2,
        'tags':[
            'bot_id:build265-m2',
            'build_type:tryjob',
            'master:False']
    }]
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     'SearchAllBuilds',
                     return_value=searched_builds)

    cancel_content = {
        'kind': 'kind',
        'etag': 'etag',
        'results':[{
            'build_id': buildbucket_id_1,
            'build': {
                'status': 'COMPLETED',
                'result': 'CANCELED',
            }
        }, {
            'build_id': buildbucket_id_2,
            'error': {
                'message': "Cannot cancel a completed build",
                'reason': 'BUILD_IS_COMPLETED',
            }
        }]
    }
    cancel_mock = self.PatchObject(buildbucket_lib.BuildbucketClient,
                                   'CancelBatchBuildsRequest',
                                   return_value=cancel_content)

    stage = self.ConstructStage()
    stage.CancelObsoleteSlaveBuilds()

    self.assertEqual(cancel_mock.call_count, 1)

  def testNoObsoleteSlaveBuilds(self):
    """Test no obsolete slave builds."""
    search_content = {
        'kind': 'kind',
        'etag': 'etag'
    }
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     'SearchBuildsRequest',
                     return_value=search_content)

    cancel_mock = self.PatchObject(buildbucket_lib.BuildbucketClient,
                                   'CancelBatchBuildsRequest')

    stage = self.ConstructStage()
    stage.CancelObsoleteSlaveBuilds()

    self.assertEqual(cancel_mock.call_count, 0)

  def testCancelObsoleteSlaveBuildsWithNoSlaveBuilds(self):
    """Test CancelObsoleteSlaveBuilds with no slave builds."""
    self.PatchObject(build_stages.CleanUpStage,
                     '_GetBuildbucketBucketsForSlaves',
                     return_value=set())
    stage = self.ConstructStage()
    stage.CancelObsoleteSlaveBuilds()

    search_mock = self.PatchObject(buildbucket_lib.BuildbucketClient,
                                   'SearchAllBuilds')
    search_mock.assertNotCalled()
