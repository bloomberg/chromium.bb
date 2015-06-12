# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for test stages."""

from __future__ import print_function

import mock
import os

from chromite.cbuildbot import cbuildbot_unittest
from chromite.cbuildbot import commands
from chromite.cbuildbot import config_lib
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import lab_status
from chromite.cbuildbot.stages import artifact_stages
from chromite.cbuildbot.stages import test_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.lib import cgroups
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import timeout_util


# pylint: disable=too-many-ancestors


class VMTestStageTest(generic_stages_unittest.AbstractStageTestCase,
                      cbuildbot_unittest.SimpleBuilderTestCase):
  """Tests for the VMTest stage."""

  BOT_ID = 'x86-generic-full'
  RELEASE_TAG = ''

  def setUp(self):
    for cmd in ('RunTestSuite', 'CreateTestRoot', 'GenerateStackTraces',
                'ArchiveFile', 'ArchiveTestResults', 'ArchiveVMFiles',
                'UploadArchivedFile', 'RunDevModeTest', 'RunCrosVMTest',
                'ListFailedTests', 'GetTestResultsDir',
                'BuildAndArchiveTestResultsTarball'):
      self.PatchObject(commands, cmd, autospec=True)
    self.PatchObject(test_stages.VMTestStage, '_NoTestResults',
                     autospec=True, return_value=False)
    self.PatchObject(osutils, 'RmDir', autospec=True)
    self.PatchObject(cgroups, 'SimpleContainChildren', autospec=True)
    self._Prepare()

    # Simulate breakpad symbols being ready.
    board_runattrs = self._run.GetBoardRunAttrs(self._current_board)
    board_runattrs.SetParallel('breakpad_symbols_generated', True)

  def ConstructStage(self):
    # pylint: disable=W0212
    self._run.GetArchive().SetupArchivePath()
    stage = test_stages.VMTestStage(self._run, self._current_board)
    return stage

  def testFullTests(self):
    """Tests if full unit and cros_au_test_harness tests are run correctly."""
    self._run.config['vm_tests'] = [constants.FULL_AU_TEST_TYPE]
    self.RunStage()

  def testQuickTests(self):
    """Tests if quick unit and cros_au_test_harness tests are run correctly."""
    self._run.config['vm_tests'] = [constants.SIMPLE_AU_TEST_TYPE]
    self.RunStage()

  def testFailedTest(self):
    """Tests if quick unit and cros_au_test_harness tests are run correctly."""
    self.PatchObject(test_stages.VMTestStage, '_RunTest',
                     autospec=True, side_effect=Exception())
    self.assertRaises(failures_lib.StepFailure, self.RunStage)

  def testRaisesInfraFail(self):
    """Tests that a infra failures has been raised."""
    commands.BuildAndArchiveTestResultsTarball.side_effect = (
        OSError('Cannot archive'))
    stage = self.ConstructStage()
    self.assertRaises(failures_lib.InfrastructureFailure, stage.PerformStage)


class UnitTestStageTest(generic_stages_unittest.AbstractStageTestCase):
  """Tests for the UnitTest stage."""

  BOT_ID = 'x86-generic-full'

  def setUp(self):
    self.rununittests_mock = self.PatchObject(commands, 'RunUnitTests')
    self.testauzip_mock = self.PatchObject(commands, 'TestAuZip')

    self.image_dir = os.path.join(
        self.build_root, 'src/build/images/x86-generic/latest-cbuildbot')

    self._Prepare()

  def ConstructStage(self):
    return test_stages.UnitTestStage(self._run, self._current_board)

  def testFullTests(self):
    """Tests if full unit and cros_au_test_harness tests are run correctly."""
    exists_mock = self.PatchObject(os.path, 'exists', return_value=True)

    self.RunStage()
    exists_mock.assert_called_once_with(
        os.path.join(self.image_dir, 'au-generator.zip'))
    self.rununittests_mock.assert_called_once_with(
        self.build_root, self._current_board, blacklist=[], extra_env=mock.ANY)
    self.testauzip_mock.assert_called_once_with(self.build_root, self.image_dir)


class HWTestStageTest(generic_stages_unittest.AbstractStageTestCase,
                      cbuildbot_unittest.SimpleBuilderTestCase):
  """Tests for the HWTest stage."""

  BOT_ID = 'x86-mario-release'
  VERSION = 'R36-5760.0.0'
  RELEASE_TAG = ''

  def setUp(self):
    self.lab_status_mock = self.PatchObject(lab_status, 'CheckLabStatus')
    self.run_suite_mock = self.PatchObject(commands, 'RunHWTestSuite')
    self.warning_mock = self.PatchObject(
        cros_build_lib, 'PrintBuildbotStepWarnings')
    self.failure_mock = self.PatchObject(
        cros_build_lib, 'PrintBuildbotStepFailure')

    self.suite_config = None
    self.suite = None
    self.version = None

    self._Prepare()

  def _Prepare(self, bot_id=None, version=None, warn_only=False, **kwargs):
    super(HWTestStageTest, self)._Prepare(bot_id, **kwargs)

    self.version = version or self.VERSION
    self._run.options.log_dir = '/b/cbuild/mylogdir'
    self.suite_config = self.GetHWTestSuite()
    self.suite_config.warn_only = warn_only
    self.suite = self.suite_config.suite

  def ConstructStage(self):
    self._run.GetArchive().SetupArchivePath()
    board_runattrs = self._run.GetBoardRunAttrs(self._current_board)
    board_runattrs.SetParallelDefault('payloads_generated', True)
    return test_stages.HWTestStage(
        self._run, self._current_board, self.suite_config)

  def _RunHWTestSuite(self, debug=False, fails=False, warns=False,
                      cmd_fail_mode=None):
    """Verify the stage behavior in various circumstances.

    Args:
      debug: Whether the HWTest suite should be run in debug mode.
      fails: Whether the stage should fail.
      warns: Whether the stage should warn.
      cmd_fail_mode: How commands.RunHWTestSuite() should fail.
        If None, don't fail.
    """
    # We choose to define these mocks in setUp() because they are
    # useful for tests that do not call this method. However, this
    # means we have to reset the mocks before each run.
    self.lab_status_mock.reset_mock()
    self.run_suite_mock.reset_mock()
    self.warning_mock.reset_mock()
    self.failure_mock.reset_mock()

    to_raise = None

    if cmd_fail_mode == 'timeout':
      to_raise = timeout_util.TimeoutError('Timed out')
    elif cmd_fail_mode == 'suite_timeout':
      to_raise = failures_lib.SuiteTimedOut('Suite timed out')
    elif cmd_fail_mode == 'board_not_available':
      to_raise = failures_lib.BoardNotAvailable('Board not available')
    elif cmd_fail_mode == 'lab_fail':
      to_raise = failures_lib.TestLabFailure('Test lab failure')
    elif cmd_fail_mode == 'test_warn':
      to_raise = failures_lib.TestWarning('Suite passed with warnings')
    elif cmd_fail_mode == 'test_fail':
      to_raise = failures_lib.TestFailure('HWTest failed.')
    elif cmd_fail_mode is not None:
      raise ValueError('cmd_fail_mode %s not supported' % cmd_fail_mode)

    self.run_suite_mock.side_effect = to_raise

    if fails:
      self.assertRaises(failures_lib.StepFailure, self.RunStage)
    else:
      self.RunStage()

    self.lab_status_mock.assert_called_once()

    self.run_suite_mock.assert_called_once()
    self.assertEqual(self.run_suite_mock.call_args[1].get('debug'), debug)

    # Make sure we print the buildbot failure/warning messages correctly.
    if fails:
      self.failure_mock.assert_called_once()
    else:
      self.assertFalse(self.failure_mock.called)

    if warns:
      self.warning_mock.assert_called_once()
    else:
      self.assertFalse(self.warning_mock.called)

  def testRemoteTrybotWithHWTest(self):
    """Test remote trybot with hw test enabled"""
    cmd_args = ['--remote-trybot', '-r', self.build_root, '--hwtest']
    self._Prepare(cmd_args=cmd_args)
    self._RunHWTestSuite()

  def testRemoteTrybotNoHWTest(self):
    """Test remote trybot with no hw test"""
    cmd_args = ['--remote-trybot', '-r', self.build_root]
    self._Prepare(cmd_args=cmd_args)
    self._RunHWTestSuite(debug=True)

  def testWithSuite(self):
    """Test if run correctly with a test suite."""
    self._RunHWTestSuite()

  def testHandleTestWarning(self):
    """Tests that we pass the build on test warning."""
    # CQ passes.
    self._Prepare('x86-alex-paladin')
    self._RunHWTestSuite(warns=True, cmd_fail_mode='test_warn')

    # PFQ passes.
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(warns=True, cmd_fail_mode='test_warn')

    # Canary passes.
    self._Prepare('x86-alex-release')
    self._RunHWTestSuite(warns=True, cmd_fail_mode='test_warn')

  def testHandleLabFail(self):
    """Tests that we handle lab failures correctly."""
    # CQ fails.
    self._Prepare('x86-alex-paladin')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='lab_fail')

    # PFQ fails.
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='lab_fail')

    # Canary fails.
    self._Prepare('x86-alex-release')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='lab_fail')

  def testWithSuiteWithFatalFailure(self):
    """Tests that we fail on test failure."""
    self._RunHWTestSuite(fails=True, cmd_fail_mode='test_fail')

  def testWithSuiteWithFatalFailureWarnFlag(self):
    """Tests that we don't fail if HWTestConfig warn_only is True."""
    self._Prepare('x86-alex-release', warn_only=True)
    self._RunHWTestSuite(warns=True, cmd_fail_mode='test_fail')

  def testHandleSuiteTimeout(self):
    """Tests that we handle suite timeout correctly ."""
    # Canary fails.
    self._Prepare('x86-alex-release')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='suite_timeout')

    # CQ fails.
    self._Prepare('x86-alex-paladin')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='suite_timeout')

    # PFQ fails.
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='suite_timeout')

  def testHandleBoardNotAvailable(self):
    """Tests that we handle board not available correctly."""
    # Canary passes.
    self._Prepare('x86-alex-release')
    self._RunHWTestSuite(warns=True, cmd_fail_mode='board_not_available')

    # CQ fails.
    self._Prepare('x86-alex-paladin')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='board_not_available')

    # PFQ fails.
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='board_not_available')

  def testHandleTimeout(self):
    """Tests that we handle timeout exceptions correctly."""
    # Canary fails.
    self._Prepare('x86-alex-release')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='timeout')

    # CQ fails.
    self._Prepare('x86-alex-paladin')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='timeout')

    # PFQ fails.
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='timeout')

  def testHandleLabDownAsFatal(self):
    """Test that the stage fails when lab is down."""
    self._Prepare('lumpy-paladin')
    self.lab_status_mock.side_effect = lab_status.LabIsDownException(
        'Lab is not up.')
    self.assertRaises(failures_lib.StepFailure, self.RunStage)

  def testPayloadsNotGenerated(self):
    """Test that we exit early if payloads are not generated."""
    board_runattrs = self._run.GetBoardRunAttrs(self._current_board)
    board_runattrs.SetParallel('payloads_generated', False)

    self.RunStage()

    # Make sure we make the stage orange.
    self.warning_mock.assert_called_once()
    # We exit early, so commands.RunHWTestSuite should not have been
    # called.
    self.assertFalse(self.run_suite_mock.called)

  def testBranchedBuildExtendsTimeouts(self):
    """Tests that we run with an extended timeout on a branched build."""
    cmd_args = ['--branch', 'notTot', '-r', self.build_root,
                '--remote-trybot', '--hwtest']
    self._Prepare('x86-alex-release', cmd_args=cmd_args)
    self._RunHWTestSuite()
    self.assertEqual(self.suite_config.timeout,
                     config_lib.HWTestConfig.BRANCHED_HW_TEST_TIMEOUT)
    self.assertEqual(self.suite_config.priority,
                     constants.HWTEST_DEFAULT_PRIORITY)


class AUTestStageTest(generic_stages_unittest.AbstractStageTestCase,
                      cros_build_lib_unittest.RunCommandTestCase,
                      cbuildbot_unittest.SimpleBuilderTestCase):
  """Test only custom methods in AUTestStageTest."""

  BOT_ID = 'x86-mario-release'
  RELEASE_TAG = '0.0.1'

  # pylint: disable=W0201
  def setUp(self):
    self.PatchObject(commands, 'ArchiveFile', autospec=True,
                     return_value='foo.txt')
    self.PatchObject(lab_status, 'CheckLabStatus', autospec=True)

    self.archive_stage = None
    self.suite_config = None
    self.suite = None

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(AUTestStageTest, self)._Prepare(bot_id, **kwargs)

    self._run.GetArchive().SetupArchivePath()
    self.archive_stage = artifact_stages.ArchiveStage(self._run,
                                                      self._current_board)
    self.suite_config = self.GetHWTestSuite()
    self.suite = self.suite_config.suite

  def ConstructStage(self):
    board_runattrs = self._run.GetBoardRunAttrs(self._current_board)
    board_runattrs.SetParallelDefault('payloads_generated', True)
    board_runattrs.SetParallelDefault('delta_payloads_generated', True)
    return test_stages.AUTestStage(
        self._run, self._current_board, self.suite_config)

  def testPerformStage(self):
    """Tests that we correctly generate a tarball and archive it."""
    stage = self.ConstructStage()
    stage.PerformStage()
    cmd = ['site_utils/autoupdate/full_release_test.py', '--npo', '--dump',
           '--archive_url', self.archive_stage.upload_url,
           self.archive_stage.release_tag, self._current_board]
    self.assertCommandContains(cmd)
    # pylint: disable=W0212
    self.assertCommandContains([commands._AUTOTEST_RPC_CLIENT, self.suite])

  def testPayloadsNotGenerated(self):
    """Test that we exit early if payloads are not generated."""
    board_runattrs = self._run.GetBoardRunAttrs(self._current_board)
    board_runattrs.SetParallel('delta_payloads_generated', False)
    self.warning_mock = self.PatchObject(
        cros_build_lib, 'PrintBuildbotStepWarnings')
    self.run_suite_mock = self.PatchObject(commands, 'RunHWTestSuite')

    self.RunStage()

    # Make sure we make the stage orange.
    self.warning_mock.assert_called_once()
    # We exit early, so commands.RunHWTestSuite should not have been
    # called.
    self.assertFalse(self.run_suite_mock.called)


class ImageTestStageTest(generic_stages_unittest.AbstractStageTestCase,
                         cros_build_lib_unittest.RunCommandTestCase,
                         cbuildbot_unittest.SimpleBuilderTestCase):
  """Test image test stage."""

  BOT_ID = 'x86-mario-release'
  RELEASE_TAG = 'ToT.0.0'

  def setUp(self):
    self._test_root = os.path.join(self.build_root, 'tmp/results_dir')
    self.PatchObject(commands, 'CreateTestRoot', autospec=True,
                     return_value='/tmp/results_dir')
    self.PatchObject(path_util, 'ToChrootPath',
                     side_effect=lambda x: x)
    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(ImageTestStageTest, self)._Prepare(bot_id, **kwargs)
    self._run.GetArchive().SetupArchivePath()

  def ConstructStage(self):
    return test_stages.ImageTestStage(self._run, self._current_board)

  def testPerformStage(self):
    """Tests that we correctly run test-image script."""
    stage = self.ConstructStage()
    stage.PerformStage()
    cmd = [
        'sudo', '--',
        os.path.join(self.build_root, 'chromite', 'bin', 'test_image'),
        '--board', self._current_board,
        '--test_results_root',
        path_util.ToChrootPath(os.path.join(self._test_root,
                                            'image_test_results')),
        path_util.ToChrootPath(stage.GetImageDirSymlink()),
    ]
    self.assertCommandContains(cmd)
