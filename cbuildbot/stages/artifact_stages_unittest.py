# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the artifact stages."""

from __future__ import print_function

import json
import os
import sys

import mock

from chromite.cbuildbot import cbuildbot_unittest
from chromite.cbuildbot import commands
from chromite.lib import constants
from chromite.lib import failures_lib
from chromite.cbuildbot import prebuilts
from chromite.lib import results_lib
from chromite.cbuildbot.stages import artifact_stages
from chromite.cbuildbot.stages import build_stages_unittest
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock
from chromite.lib import path_util
from chromite.lib.buildstore import FakeBuildStore

from chromite.cbuildbot.stages.generic_stages_unittest import patch
from chromite.cbuildbot.stages.generic_stages_unittest import patches

# pylint: disable=too-many-ancestors


class ArchiveStageTest(generic_stages_unittest.AbstractStageTestCase,
                       cbuildbot_unittest.SimpleBuilderTestCase):
  """Exercise ArchiveStage functionality."""

  # pylint: disable=protected-access

  RELEASE_TAG = ''
  VERSION = '3333.1.0'

  def _PatchDependencies(self):
    """Patch dependencies of ArchiveStage.PerformStage()."""
    to_patch = [(parallel, 'RunParallelSteps'), (commands, 'PushImages'),
                (commands, 'UploadArchivedFile')]
    self.AutoPatch(to_patch)

  def setUp(self):
    self._PatchDependencies()

    self._Prepare()
    self.buildstore = FakeBuildStore()

  def _Prepare(self, bot_id=None, **kwargs):
    extra_config = {'upload_symbols': True, 'push_image': True}
    super(ArchiveStageTest, self)._Prepare(
        bot_id, extra_config=extra_config, **kwargs)

  def ConstructStage(self):
    self._run.GetArchive().SetupArchivePath()
    return artifact_stages.ArchiveStage(self._run, self.buildstore,
                                        self._current_board)

  def testArchive(self):
    """Simple did-it-run test."""
    # TODO(davidjames): Test the individual archive steps as well.
    self.RunStage()

  # TODO(build): This test is not actually testing anything real.  It confirms
  # that PushImages is not called, but the mock for RunParallelSteps already
  # prevents PushImages from being called, regardless of whether this is a
  # trybot flow.
  def testNoPushImagesForRemoteTrybot(self):
    """Test that remote trybot overrides work to disable push images."""
    self._Prepare(cmd_args=[
        '--remote-trybot', '-r', self.build_root, '--buildnumber=1234',
        'eve-release'
    ])
    self.RunStage()
    # pylint: disable=no-member
    self.assertEqual(commands.PushImages.call_count, 0)

  def ConstructStageForArchiveStep(self):
    """Stage construction for archive steps."""
    stage = self.ConstructStage()
    self.PatchObject(stage._upload_queue, 'put', autospec=True)
    self.PatchObject(path_util, 'ToChrootPath', return_value='', autospec=True)
    return stage


class UploadPrebuiltsStageTest(
    generic_stages_unittest.RunCommandAbstractStageTestCase,
    cbuildbot_unittest.SimpleBuilderTestCase):
  """Tests for the UploadPrebuilts stage."""

  cmd = 'upload_prebuilts'
  RELEASE_TAG = ''

  def _Prepare(self, bot_id=None, **kwargs):
    super(UploadPrebuiltsStageTest, self)._Prepare(bot_id, **kwargs)
    self.cmd = os.path.join(self.build_root, constants.CHROMITE_BIN_SUBDIR,
                            'upload_prebuilts')
    self._run.options.prebuilts = True
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    return artifact_stages.UploadPrebuiltsStage(self._run, self.buildstore,
                                                self._run.config.boards[-1])

  def _VerifyBoardMap(self,
                      bot_id,
                      count,
                      board_map,
                      public_args=None,
                      private_args=None):
    """Verify that the prebuilts are uploaded for the specified bot.

    Args:
      bot_id: Bot to upload prebuilts for.
      count: Number of assert checks that should be performed.
      board_map: Map from slave boards to whether the bot is public.
      public_args: List of extra arguments for public boards.
      private_args: List of extra arguments for private boards.
    """
    self._Prepare(bot_id)
    self.RunStage()
    public_prefix = [self.cmd] + (public_args or [])
    private_prefix = [self.cmd] + (private_args or [])
    for board, public in board_map.items():
      if public or public_args:
        public_cmd = public_prefix + ['--slave-board', board]
        self.assertCommandContains(public_cmd, expected=public)
        count -= 1
      private_cmd = private_prefix + ['--slave-board', board, '--private']
      self.assertCommandContains(private_cmd, expected=not public)
      count -= 1
    if board_map:
      self.assertCommandContains(
          [self.cmd, '--set-version',
           self._run.GetVersion()],)
      count -= 1
    self.assertEqual(
        count, 0,
        'Number of asserts performed does not match (%d remaining)' % count)

  def testFullPrebuiltsUpload(self):
    """Test uploading of full builder prebuilts."""
    self._VerifyBoardMap('amd64-generic-full', 0, {})
    self.assertCommandContains([self.cmd, '--git-sync'])

  def testIncorrectCount(self):
    """Test that _VerifyBoardMap asserts when the count is wrong."""
    self.assertRaises(AssertionError, self._VerifyBoardMap,
                      'amd64-generic-full', 1, {})


class UploadDevInstallerPrebuiltsStageTest(
    generic_stages_unittest.AbstractStageTestCase,
    cbuildbot_unittest.SimpleBuilderTestCase):
  """Tests for the UploadDevInstallerPrebuilts stage."""

  RELEASE_TAG = 'RT'

  def setUp(self):
    self.upload_mock = self.PatchObject(prebuilts,
                                        'UploadDevInstallerPrebuilts')

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(UploadDevInstallerPrebuiltsStageTest, self)._Prepare(bot_id, **kwargs)

    self._run.options.chrome_rev = None
    self._run.options.prebuilts = True
    self._run.config['dev_installer_prebuilts'] = True
    self._run.config['binhost_bucket'] = 'gs://testbucket'
    self._run.config['binhost_key'] = 'dontcare'
    self._run.config['binhost_base_url'] = 'https://dontcare/here'
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    return artifact_stages.DevInstallerPrebuiltsStage(
        self._run, self.buildstore, self._current_board)

  def testDevInstallerUpload(self):
    """Basic sanity test testing uploads of dev installer prebuilts."""
    self.RunStage()

    self.upload_mock.assert_called_with(
        binhost_bucket=self._run.config.binhost_bucket,
        binhost_key=self._run.config.binhost_key,
        binhost_base_url=self._run.config.binhost_base_url,
        buildroot=self.build_root,
        board=self._current_board,
        extra_args=mock.ANY)


class CPEExportStageTest(generic_stages_unittest.AbstractStageTestCase,
                         cbuildbot_unittest.SimpleBuilderTestCase):
  """Test CPEExportStage"""

  def setUp(self):
    self.CreateMockOverlay('amd64-generic')

    self.StartPatcher(generic_stages_unittest.ArchivingStageMixinMock())
    self.StartPatcher(parallel_unittest.ParallelMock())

    self.rc_mock = self.StartPatcher(cros_test_lib.RunCommandMock())
    self.rc_mock.SetDefaultCmdResult(output='')

    self.stage = None
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    """Create a CPEExportStage instance for testing"""
    self._run.GetArchive().SetupArchivePath()
    return artifact_stages.CPEExportStage(self._run, self.buildstore,
                                          self._current_board)

  def assertBoardAttrEqual(self, attr, expected_value):
    """Assert the value of a board run |attr| against |expected_value|."""
    value = self.stage.board_runattrs.GetParallel(attr)
    self.assertEqual(expected_value, value)

  def _TestPerformStage(self):
    """Run PerformStage for the stage."""
    self._Prepare()
    self._run.attrs.release_tag = self.VERSION

    self.stage = self.ConstructStage()
    self.stage.PerformStage()

  def testCPEExport(self):
    """Test that CPEExport stage runs without syntax errors."""
    self._TestPerformStage()


class DebugSymbolsStageTest(generic_stages_unittest.AbstractStageTestCase,
                            cbuildbot_unittest.SimpleBuilderTestCase):
  """Test DebugSymbolsStage"""

  # pylint: disable=protected-access

  def setUp(self):
    self.CreateMockOverlay('amd64-generic')

    self.StartPatcher(generic_stages_unittest.ArchivingStageMixinMock())

    self.gen_mock = self.PatchObject(commands, 'GenerateBreakpadSymbols')
    self.gen_android_mock = self.PatchObject(commands,
                                             'GenerateAndroidBreakpadSymbols')
    self.upload_mock = self.PatchObject(commands, 'UploadSymbols')
    self.tar_mock = self.PatchObject(commands, 'GenerateDebugTarball')

    self.rc_mock = self.StartPatcher(cros_test_lib.RunCommandMock())
    self.rc_mock.SetDefaultCmdResult(output='')

    self.stage = None

  def _Prepare(self, extra_config=None, **kwargs):
    """Prepare this stage for testing."""
    if extra_config is None:
      extra_config = {
          'archive_build_debug': True,
          'vm_tests': True,
          'upload_symbols': True,
      }
    super(DebugSymbolsStageTest, self)._Prepare(
        extra_config=extra_config, **kwargs)
    self._run.attrs.release_tag = self.VERSION
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    """Create a DebugSymbolsStage instance for testing"""
    self._run.GetArchive().SetupArchivePath()
    return artifact_stages.DebugSymbolsStage(self._run, self.buildstore,
                                             self._current_board)

  def assertBoardAttrEqual(self, attr, expected_value):
    """Assert the value of a board run |attr| against |expected_value|."""
    value = self.stage.board_runattrs.GetParallel(attr)
    self.assertEqual(expected_value, value)

  def _TestPerformStage(self,
                        extra_config=None,
                        create_android_symbols_archive=False):
    """Run PerformStage for the stage with the given extra config."""
    self._Prepare(extra_config=extra_config)

    self.tar_mock.side_effect = '/my/tar/ball'
    self.stage = self.ConstructStage()

    if create_android_symbols_archive:
      symbols_file = os.path.join(self.stage.archive_path,
                                  constants.ANDROID_SYMBOLS_FILE)
      osutils.Touch(symbols_file)

    try:
      self.stage.PerformStage()
    except Exception:
      return self.stage._HandleStageException(sys.exc_info())

  def testPerformStageWithSymbols(self):
    """Smoke test for an PerformStage when debugging is enabled"""
    self._TestPerformStage()

    self.assertEqual(self.gen_mock.call_count, 1)
    self.assertEqual(self.gen_android_mock.call_count, 0)
    self.assertEqual(self.upload_mock.call_count, 1)
    self.assertEqual(self.tar_mock.call_count, 2)

    self.assertBoardAttrEqual('breakpad_symbols_generated', True)
    self.assertBoardAttrEqual('debug_tarball_generated', True)

  def testPerformStageWithAndroidSymbols(self):
    """Smoke test for an PerformStage when Android symbols are available"""
    self._TestPerformStage(create_android_symbols_archive=True)

    self.assertEqual(self.gen_mock.call_count, 1)
    self.assertEqual(self.gen_android_mock.call_count, 1)
    self.assertEqual(self.upload_mock.call_count, 1)
    self.assertEqual(self.tar_mock.call_count, 2)

    self.assertBoardAttrEqual('breakpad_symbols_generated', True)
    self.assertBoardAttrEqual('debug_tarball_generated', True)

  def testPerformStageNoSymbols(self):
    """Smoke test for an PerformStage when debugging is disabled"""
    extra_config = {
        'archive_build_debug': False,
        'vm_tests': False,
        'upload_symbols': False,
    }
    result = self._TestPerformStage(extra_config)
    self.assertIsNone(result)

    self.assertEqual(self.gen_mock.call_count, 1)
    self.assertEqual(self.gen_android_mock.call_count, 0)
    self.assertEqual(self.upload_mock.call_count, 0)
    self.assertEqual(self.tar_mock.call_count, 2)

    self.assertBoardAttrEqual('breakpad_symbols_generated', True)
    self.assertBoardAttrEqual('debug_tarball_generated', True)

  def testGenerateCrashStillNotifies(self):
    """Crashes in symbol generation should still notify external events."""

    class TestError(Exception):
      """Unique test exception"""

    self.gen_mock.side_effect = TestError('mew')
    result = self._TestPerformStage()
    self.assertIsInstance(result[0], failures_lib.InfrastructureFailure)

    self.assertEqual(self.gen_mock.call_count, 1)
    self.assertEqual(self.gen_android_mock.call_count, 0)
    self.assertEqual(self.upload_mock.call_count, 0)
    self.assertEqual(self.tar_mock.call_count, 0)

    self.assertBoardAttrEqual('breakpad_symbols_generated', False)
    self.assertBoardAttrEqual('debug_tarball_generated', False)

  def testUploadCrashStillNotifies(self):
    """Crashes in symbol upload should still notify external events."""
    self.upload_mock.side_effect = failures_lib.BuildScriptFailure(
        cros_build_lib.RunCommandError('mew', cros_build_lib.CommandResult()),
        'mew')
    result = self._TestPerformStage()
    self.assertIs(result[0], results_lib.Results.FORGIVEN)

    self.assertBoardAttrEqual('breakpad_symbols_generated', True)
    self.assertBoardAttrEqual('debug_tarball_generated', True)

  def testUploadCrashUploadsList(self):
    """A crash in symbol upload should still post the failed list file."""
    self.upload_mock.side_effect = failures_lib.BuildScriptFailure(
        cros_build_lib.RunCommandError('mew', cros_build_lib.CommandResult()),
        'mew')
    self._Prepare()
    stage = self.ConstructStage()

    with mock.patch.object(os.path, 'exists') as mock_exists, \
         mock.patch.object(artifact_stages.DebugSymbolsStage,
                           'UploadArtifact') as mock_upload:
      mock_exists.return_value = True
      self.assertRaises(artifact_stages.DebugSymbolsUploadException,
                        stage.UploadSymbols, stage._build_root,
                        stage._current_board)
      self.assertEqual(mock_exists.call_count, 1)
      self.assertEqual(mock_upload.call_count, 1)


class UploadTestArtifactsStageMock(
    generic_stages_unittest.ArchivingStageMixinMock):
  """Partial mock for BuildImageStage."""

  TARGET = 'chromite.cbuildbot.stages.artifact_stages.UploadTestArtifactsStage'
  ATTRS = (
      generic_stages_unittest.ArchivingStageMixinMock.ATTRS +
      ('BuildAutotestTarballs', 'BuildTastTarball',
       'BuildGuestImagesTarball'))

  def BuildAutotestTarballs(self, *args, **kwargs):
    with patches(
        patch(commands, 'BuildTarball'),
        patch(commands, 'FindFilesWithPattern', return_value=['foo.txt'])):
      self.backup['BuildAutotestTarballs'](*args, **kwargs)

  def BuildTastTarball(self, *args, **kwargs):
    with patch(commands, 'BuildTarball'):
      self.backup['BuildTastTarball'](*args, **kwargs)

  def BuildGuestImagesTarball(self, *args, **kwargs):
    self.backup['BuildGuestImagesTarball'](*args, **kwargs)

class UploadTestArtifactsStageTest(build_stages_unittest.AllConfigsTestCase,
                                   cbuildbot_unittest.SimpleBuilderTestCase):
  """Tests UploadTestArtifactsStage."""

  def setUp(self):
    self._release_tag = None

    osutils.SafeMakedirs(os.path.join(self.build_root, 'chroot', 'tmp'))
    self.StartPatcher(UploadTestArtifactsStageMock())
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    return artifact_stages.UploadTestArtifactsStage(self._run, self.buildstore,
                                                    self._current_board)

  def RunTestsWithBotId(self, bot_id, options_tests=True):
    """Test with the config for the specified bot_id."""
    self._Prepare(bot_id)
    self._run.options.tests = options_tests
    self._run.attrs.release_tag = '0.0.1'

    # Simulate images being ready.
    board_runattrs = self._run.GetBoardRunAttrs(self._current_board)
    board_runattrs.SetParallel('images_generated', True)

    generate_quick_provision_payloads_mock = self.PatchObject(
        commands, 'GenerateQuickProvisionPayloads')
    generate_update_payloads_mock = self.PatchObject(commands,
                                                     'GeneratePayloads')

    with parallel_unittest.ParallelMock():
      with self.RunStageWithConfig():
        if (self._run.config.upload_hw_test_artifacts and
            self._run.config.images):
          self.assertNotEqual(generate_update_payloads_mock.call_count, 0)
          self.assertNotEqual(generate_quick_provision_payloads_mock.call_count,
                              0)
        else:
          self.assertEqual(generate_update_payloads_mock.call_count, 0)
          self.assertEqual(generate_quick_provision_payloads_mock.call_count, 0)

  def testAllConfigs(self):
    """Test all major configurations"""
    self.RunAllConfigs(self.RunTestsWithBotId)


# TODO: Delete ArchivingMock once ArchivingStage is deprecated.
class ArchivingMock(partial_mock.PartialMock):
  """Partial mock for ArchivingStage."""

  TARGET = 'chromite.cbuildbot.stages.artifact_stages.ArchivingStage'
  ATTRS = ('UploadArtifact',)

  def UploadArtifact(self, *args, **kwargs):
    with patch(commands, 'ArchiveFile', return_value='foo.txt'):
      with patch(commands, 'UploadArchivedFile'):
        self.backup['UploadArtifact'](*args, **kwargs)


# TODO: Delete ArchivingStageTest once ArchivingStage is deprecated.
class ArchivingStageTest(generic_stages_unittest.AbstractStageTestCase,
                         cbuildbot_unittest.SimpleBuilderTestCase):
  """Excerise ArchivingStage functionality."""
  RELEASE_TAG = ''

  def setUp(self):
    self.StartPatcher(ArchivingMock())

    self._Prepare()
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    self._run.GetArchive().SetupArchivePath()
    archive_stage = artifact_stages.ArchiveStage(self._run, self.buildstore,
                                                 self._current_board)
    return artifact_stages.ArchivingStage(self._run, self.buildstore,
                                          self._current_board, archive_stage)


class GenerateSysrootStageTest(generic_stages_unittest.AbstractStageTestCase,
                               cbuildbot_unittest.SimpleBuilderTestCase):
  """Exercise GenerateSysrootStage functionality."""

  RELEASE_TAG = ''

  # pylint: disable=protected-access

  def setUp(self):
    self._Prepare()
    self.rc_mock = self.StartPatcher(cros_test_lib.RunCommandMock())
    self.rc_mock.SetDefaultCmdResult()
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    self._run.GetArchive().SetupArchivePath()
    return artifact_stages.GenerateSysrootStage(self._run, self.buildstore,
                                                self._current_board)

  def testGenerateSysroot(self):
    """Test that the sysroot generation was called correctly."""
    stage = self.ConstructStage()
    self.PatchObject(path_util, 'ToChrootPath', return_value='', autospec=True)
    self.PatchObject(stage._upload_queue, 'put', autospec=True)
    stage._GenerateSysroot()
    sysroot_tarball = 'sysroot_%s.tar.xz' % ('virtual_target-os')
    stage._upload_queue.put.assert_called_with([sysroot_tarball])


class CollectPGOProfilesStageTest(generic_stages_unittest.AbstractStageTestCase,
                                  cbuildbot_unittest.SimpleBuilderTestCase):
  """Exercise CollectPGOProfilesStage functionality."""

  RELEASE_TAG = ''

  _VALID_CLANG_VERSION_SHA = 'de7a0a152648d1a74cf4319920b1848aa00d1ca3'
  _VALID_CLANG_VERSION_STRING = (
      'Chromium OS 9.0_pre353983_p20190325-r13 clang version 9.0.0 '
      '(/var/cache/chromeos-cache/distfiles/host/egit-src/llvm-project '
      'de7a0a152648d1a74cf4319920b1848aa00d1ca3) (based on LLVM 9.0.0svn)\n'
      'Target: x86_64-pc-linux-gnu\n'
      'Thread model: posix\n'
      'InstalledDir: /usr/bin\n'
  )

  # pylint: disable=protected-access

  def setUp(self):
    self._Prepare()
    self.rc_mock = self.StartPatcher(cros_test_lib.RunCommandMock())
    self.rc_mock.SetDefaultCmdResult()
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    self._run.GetArchive().SetupArchivePath()
    return artifact_stages.CollectPGOProfilesStage(self._run, self.buildstore,
                                                   self._current_board)

  def testParseLLVMHeadSHA(self):
    stage = self.ConstructStage()
    actual_sha = stage._ParseLLVMHeadSHA(self._VALID_CLANG_VERSION_STRING)
    self.assertEqual(actual_sha, self._VALID_CLANG_VERSION_SHA)

  def _MetadataMultiDispatch(self, equery_uses_fn, clang_version_fn):
    def result(command, enter_chroot, redirect_stdout):
      self.assertTrue(enter_chroot)
      self.assertTrue(redirect_stdout)

      if command == ['equery', '-C', '-N', 'uses', 'sys-devel/llvm']:
        stdout = equery_uses_fn()
      elif command == ['clang', '--version']:
        stdout = clang_version_fn()
      else:
        raise ValueError('Unexpected command: %s' % command)

      return cros_build_lib.CommandResult(output=stdout)

    return result

  def testCollectLLVMMetadataRaisesOnAnInvalidVersionString(self):
    stage = self.ConstructStage()

    def equery_uses():
      return ' - + llvm_pgo_generate :'

    def clang_version():
      valid_version_lines = self._VALID_CLANG_VERSION_STRING.splitlines()
      valid_version_lines[0] = 'clang version 8.0.1\n'
      return ''.join(valid_version_lines)

    with patch(cros_build_lib, 'RunCommand') as run_command:
      run_command.side_effect = self._MetadataMultiDispatch(
          equery_uses_fn=equery_uses,
          clang_version_fn=clang_version)

      with self.assertRaises(ValueError) as raised:
        stage._CollectLLVMMetadata()

      self.assertIn('version string', raised.exception.message)

  def testCollectLLVMMetadataRaisesIfClangIsntPGOGenerated(self):
    stage = self.ConstructStage()

    def clang_version():
      return self._VALID_CLANG_VERSION_STRING

    with patch(cros_build_lib, 'RunCommand') as run_command:
      for uses in ['', ' - - llvm_pgo_generate :']:
        def equery_uses():
          # We're using a loop var on purpose; this function should die by the
          # end of the current iteration.
          # pylint: disable=cell-var-from-loop
          return uses

        run_command.side_effect = self._MetadataMultiDispatch(
            equery_uses_fn=equery_uses,
            clang_version_fn=clang_version)

        with self.assertRaises(ValueError) as raised:
          stage._CollectLLVMMetadata()

        self.assertIn('pgo_generate flag', raised.exception.message)

  def testCollectLLVMMetadataFunctionsInASimpleCase(self):
    def clang_version():
      return self._VALID_CLANG_VERSION_STRING

    def equery_uses():
      return ' - + llvm_pgo_generate :'

    stage = self.ConstructStage()

    run_command = self.PatchObject(cros_build_lib, 'RunCommand')
    run_command.side_effect = self._MetadataMultiDispatch(equery_uses,
                                                          clang_version)
    write_file = self.PatchObject(osutils, 'WriteFile')
    upload_queue_put = self.PatchObject(stage._upload_queue, 'put')
    stage._CollectLLVMMetadata()

    write_file.assert_called_once()
    upload_queue_put.assert_called_once()

    (written_file, metadata_json), kwargs = write_file.call_args
    self.assertEqual(kwargs, {})

    expected_metadata = {
        'head_sha': self._VALID_CLANG_VERSION_SHA,
    }

    given_metadata = json.loads(metadata_json)
    self.assertEqual(given_metadata, expected_metadata)

    upload_queue_put.assert_called_with([written_file])

  def testCollectPGOProfiles(self):
    """Test that the sysroot generation was called correctly."""
    stage = self.ConstructStage()

    # No profiles directory
    with self.assertRaises(Exception) as msg:
      stage._CollectPGOProfiles()
    self.assertEqual('No profile directories found.', str(msg.exception))

    # Create profiles directory
    cov_path = 'build/%s/build/coverage_data' % stage._current_board
    out_cov_path = os.path.abspath(
        os.path.join(self.build_root, 'chroot', cov_path))
    os.makedirs(os.path.join(out_cov_path, 'raw_profiles'))

    # No profraw files
    with self.assertRaises(Exception) as msg:
      stage._CollectPGOProfiles()
    self.assertEqual('No profraw files found in profiles directory.',
                     str(msg.exception))

    # Create profraw file
    profraw = os.path.join(out_cov_path, 'raw_profiles', 'a.profraw')
    with open(profraw, 'a') as f:
      f.write('123')

    # Check uploading tarball
    self.PatchObject(stage._upload_queue, 'put', autospec=True)
    stage._CollectPGOProfiles()
    llvm_profdata = path_util.ToChrootPath(
        os.path.join(stage.archive_path, 'llvm.profdata'))
    profraw_list = path_util.ToChrootPath(
        os.path.join(stage.archive_path, 'profraw_list'))
    self.assertEqual(['llvm-profdata', 'merge',
                      '-output', llvm_profdata,
                      '-f', profraw_list],
                     stage._merge_cmd)
    tarball = stage.PROFDATA_TAR
    stage._upload_queue.put.assert_called_with([tarball])
