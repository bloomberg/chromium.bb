# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for test stages."""

from __future__ import print_function

import copy
import os

import mock

from chromite.cbuildbot import cbuildbot_unittest
from chromite.cbuildbot import commands
from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import test_stages
from chromite.lib import config_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cros_test_lib
from chromite.lib import failures_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import path_util
from chromite.lib import timeout_util
from chromite.lib.buildstore import FakeBuildStore
from chromite.scripts import cbuildbot

pytestmark = cros_test_lib.pytestmark_inside_only


# pylint: disable=too-many-ancestors,protected-access


class UnitTestStageTest(generic_stages_unittest.AbstractStageTestCase,
                        cbuildbot_unittest.SimpleBuilderTestCase):
  """Tests for the UnitTest stage."""

  BOT_ID = 'amd64-generic-full'
  RELEASE_TAG = 'ToT.0.0'

  def setUp(self):
    self.rununittests_mock = self.PatchObject(commands, 'RunUnitTests')
    self.buildunittests_mock = self.PatchObject(
        commands, 'BuildUnitTestTarball', return_value='unit_tests.tar')
    self.uploadartifact_mock = self.PatchObject(
        generic_stages.ArchivingStageMixin, 'UploadArtifact')
    self.image_dir = os.path.join(
        self.build_root, 'src/build/images/amd64-generic/latest-cbuildbot')

    self._Prepare()
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    self._run.GetArchive().SetupArchivePath()
    return test_stages.UnitTestStage(self._run, self.buildstore,
                                     self._current_board)

  def testFullTests(self):
    """Tests if full unit and cros_au_test_harness tests are run correctly."""
    makedirs_mock = self.PatchObject(osutils, 'SafeMakedirs')

    board_runattrs = self._run.GetBoardRunAttrs(self._current_board)
    board_runattrs.SetParallel('test_artifacts_uploaded', True)
    self.RunStage()
    makedirs_mock.assert_called_once_with(self._run.GetArchive().archive_path)
    self.rununittests_mock.assert_called_once_with(
        self.build_root,
        self._current_board,
        blacklist=[],
        extra_env=mock.ANY,
        build_stage=True)
    self.buildunittests_mock.assert_called_once_with(
        self.build_root, self._current_board,
        self._run.GetArchive().archive_path)
    self.uploadartifact_mock.assert_called_once_with(
        'unit_tests.tar', archive=False)


class HWTestStageTest(generic_stages_unittest.AbstractStageTestCase,
                      cbuildbot_unittest.SimpleBuilderTestCase):
  """Tests for the HWTest stage."""

  eve = 'eve-release'
  VERSION = 'R36-5760.0.0'
  RELEASE_TAG = ''

  def setUp(self):
    self.run_suite_mock = self.PatchObject(commands, 'RunHWTestSuite')
    self.warning_mock = self.PatchObject(logging, 'PrintBuildbotStepWarnings')
    self.failure_mock = self.PatchObject(logging, 'PrintBuildbotStepFailure')

    self.suite_config = None
    self.suite = None
    self.version = None

    self._Prepare()
    self.buildstore = FakeBuildStore()

  # Our API here is not great when it comes to kwargs passing.
  # pylint: disable=arguments-differ
  def _Prepare(self, bot_id=None, version=None, warn_only=False, **kwargs):
    super(HWTestStageTest, self)._Prepare(bot_id, **kwargs)

    self.version = version or self.VERSION
    self._run.options.log_dir = '/b/cbuild/mylogdir'
    self.suite_config = self.GetHWTestSuite()
    self.suite_config.warn_only = warn_only
    self.suite = self.suite_config.suite
  # pylint: enable=arguments-differ

  def ConstructStage(self):
    self._run.GetArchive().SetupArchivePath()
    board_runattrs = self._run.GetBoardRunAttrs(self._current_board)
    board_runattrs.SetParallelDefault('test_artifacts_uploaded', True)
    return test_stages.HWTestStage(self._run, self.buildstore,
                                   self._current_board, self._model,
                                   self.suite_config)

  def _RunHWTestSuite(self,
                      debug=False,
                      fails=False,
                      warns=False,
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
    self.run_suite_mock.reset_mock()
    self.warning_mock.reset_mock()
    self.failure_mock.reset_mock()

    mock_report = self.PatchObject(test_stages.HWTestStage,
                                   'ReportHWTestResults')

    to_raise = None

    if cmd_fail_mode is None:
      to_raise = None
    elif cmd_fail_mode == 'timeout':
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
    else:
      raise ValueError('cmd_fail_mode %s not supported' % cmd_fail_mode)

    if cmd_fail_mode == 'timeout':
      self.run_suite_mock.side_effect = to_raise
    else:
      self.run_suite_mock.return_value = commands.HWTestSuiteResult(
          to_raise, None)

    if fails:
      self.assertRaises(failures_lib.StepFailure, self.RunStage)
    else:
      self.RunStage()

    self.run_suite_mock.assert_called_once()
    self.assertEqual(self.run_suite_mock.call_args[1].get('debug'), debug)
    self.assertEqual(self.run_suite_mock.call_args[1].get('model'), self._model)

    # Make sure we print the buildbot failure/warning messages correctly.
    if fails:
      self.failure_mock.assert_called_once()
    else:
      self.failure_mock.assert_not_called()

    if warns:
      self.warning_mock.assert_called_once()
    else:
      self.warning_mock.assert_not_called()

    mock_report.assert_not_called()

  def testRemoteTrybotWithHWTest(self):
    """Test remote trybot with hw test enabled"""
    cmd_args = [
        '--remote-trybot', '-r', self.build_root, '--hwtest', self.BOT_ID
    ]
    self._Prepare(cmd_args=cmd_args)
    self._RunHWTestSuite()

  def testRemoteTrybotNoHWTest(self):
    """Test remote trybot with no hw test"""
    cmd_args = ['--remote-trybot', '-r', self.build_root, self.BOT_ID]
    self._Prepare(cmd_args=cmd_args)
    self._RunHWTestSuite(debug=True)

  def testWithSuite(self):
    """Test if run correctly with a test suite."""
    self._RunHWTestSuite()

  def testHandleTestWarning(self):
    """Tests that we pass the build on test warning."""
    # CQ passes.
    self._Prepare('eve-paladin')
    self._RunHWTestSuite(warns=True, cmd_fail_mode='test_warn')

    # PFQ passes.
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(warns=True, cmd_fail_mode='test_warn')

    # Canary passes.
    self._Prepare('eve-release')
    self._RunHWTestSuite(warns=True, cmd_fail_mode='test_warn')

  def testHandleLabFail(self):
    """Tests that we handle lab failures correctly."""
    # CQ fails.
    self._Prepare('eve-paladin')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='lab_fail')

    # PFQ fails.
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='lab_fail')

    # Canary fails.
    self._Prepare('eve-release')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='lab_fail')

  def testWithSuiteWithFatalFailure(self):
    """Tests that we fail on test failure."""
    self._RunHWTestSuite(fails=True, cmd_fail_mode='test_fail')

  def testWithSuiteWithFatalFailureWarnFlag(self):
    """Tests that we don't fail if HWTestConfig warn_only is True."""
    self._Prepare('eve-release', warn_only=True)
    self._RunHWTestSuite(warns=True, cmd_fail_mode='test_fail')

  def testHandleSuiteTimeout(self):
    """Tests that we handle suite timeout correctly ."""
    # Canary fails.
    self._Prepare('eve-release')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='suite_timeout')

    # CQ fails.
    self._Prepare('eve-paladin')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='suite_timeout')

    # PFQ fails.
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='suite_timeout')

  def testHandleBoardNotAvailable(self):
    """Tests that we handle board not available correctly."""
    # Canary passes.
    self._Prepare('eve-release')
    self._RunHWTestSuite(warns=True, cmd_fail_mode='board_not_available')

    # CQ fails.
    self._Prepare('eve-paladin')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='board_not_available')

    # PFQ fails.
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='board_not_available')

  def testHandleTimeout(self):
    """Tests that we handle timeout exceptions correctly."""
    # Canary fails.
    self._Prepare('eve-release')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='timeout')

    # CQ fails.
    self._Prepare('eve-paladin')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='timeout')

    # PFQ fails.
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(fails=True, cmd_fail_mode='timeout')

  def testPayloadsNotGenerated(self):
    """Test that we exit early if payloads are not generated."""
    board_runattrs = self._run.GetBoardRunAttrs(self._current_board)
    board_runattrs.SetParallel('test_artifacts_uploaded', False)

    self.RunStage()

    # Make sure we make the stage orange.
    self.warning_mock.assert_called_once()
    # We exit early, so commands.RunHWTestSuite should not have been
    # called.
    self.assertFalse(self.run_suite_mock.called)

  def testPerformStageOnCQ(self):
    """Test PerformStage on CQ."""
    self._Prepare('eve-paladin')
    stage = self.ConstructStage()
    mock_report = self.PatchObject(test_stages.HWTestStage,
                                   'ReportHWTestResults')
    cmd_result = mock.Mock(to_raise=None)
    self.PatchObject(commands, 'RunHWTestSuite', return_value=cmd_result)
    stage.PerformStage()

    mock_report.assert_called_once_with(mock.ANY, mock.ANY, mock.ANY)


class ImageTestStageTest(generic_stages_unittest.AbstractStageTestCase,
                         cros_test_lib.RunCommandTestCase,
                         cbuildbot_unittest.SimpleBuilderTestCase):
  """Test image test stage."""

  BOT_ID = 'eve-release'
  RELEASE_TAG = 'ToT.0.0'

  def setUp(self):
    self._test_root = os.path.join(self.build_root, 'tmp/results_dir')
    self.PatchObject(
        commands,
        'CreateTestRoot',
        autospec=True,
        return_value='/tmp/results_dir')
    self.PatchObject(path_util, 'ToChrootPath', side_effect=lambda x: x)
    self._Prepare()
    self.buildstore = FakeBuildStore()

  # Our API here is not great when it comes to kwargs passing.
  def _Prepare(self, bot_id=None, **kwargs):  # pylint: disable=arguments-differ
    super(ImageTestStageTest, self)._Prepare(bot_id, **kwargs)
    self._run.GetArchive().SetupArchivePath()

  def ConstructStage(self):
    return test_stages.ImageTestStage(self._run, self.buildstore,
                                      self._current_board)

  def testPerformStage(self):
    """Tests that we correctly run test-image script."""
    stage = self.ConstructStage()
    stage.PerformStage()
    cmd = [
        'sudo',
        '--',
        os.path.join(self.build_root, 'chromite', 'bin', 'test_image'),
        '--board',
        self._current_board,
        '--test_results_root',
        path_util.ToChrootPath(
            os.path.join(self._test_root, 'image_test_results')),
        path_util.ToChrootPath(stage.GetImageDirSymlink()),
    ]
    self.assertCommandContains(cmd)


class CbuildbotLaunchTestEndToEndTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests for the CbuildbotLaunchTestStage."""

  def setUp(self):
    self.tryjob_mock = self.PatchObject(
        commands, 'RunLocalTryjob', autospec=True)
    self.tryjob_failure_exception = failures_lib.BuildScriptFailure(
        cros_build_lib.RunCommandError('msg'), 'cros tryjob')

    self._Prepare()
    self.buildstore = FakeBuildStore()

  def ConstructStage(self):
    return test_stages.CbuildbotLaunchTestStage(self._run, self.buildstore)

  def testFullPassRun(self):
    """Runs through CbuildbotLaunchTestStage.

    This includes 4 runs through CbuildbotLaunchTestBuildStage.
    """
    # Tryjob command will: Fail, Pass, Pass, Pass.
    self.tryjob_mock.side_effect = iter(
        [self.tryjob_failure_exception, None, None, None])

    self.RunStage()


class HWTestPlanStageTest(cros_test_lib.MockTempDirTestCase):
  """Tests for the HWTestPlanStageTest."""

  def setUp(self):
    self.buildroot = os.path.join(self.tempdir, 'buildroot')
    self.buildstore = FakeBuildStore()

  def _initConfig(self,
                  bot_id,
                  master=False,
                  extra_argv=None,
                  override_hw_test_config=None,
                  models=None):
    """Return normal options/build_config for |bot_id|"""
    site_config = config_lib.GetConfig()
    build_config = copy.deepcopy(site_config[bot_id])
    build_config['master'] = master
    build_config['important'] = False
    if models:
      build_config['models'] = models

    # Use the cbuildbot parser to create properties and populate default values.
    parser = cbuildbot._CreateParser()
    argv = (['-r', self.buildroot, '--buildbot', '--debug', '--nochromesdk'] +
            (extra_argv if extra_argv else []) + [bot_id])
    options = cbuildbot.ParseCommandLine(parser, argv)

    # Yikes.
    options.managed_chrome = build_config['sync_chrome']

    # Iterate through override and update HWTestConfig attributes.
    if override_hw_test_config:
      for key, val in override_hw_test_config.items():
        for hw_test in build_config.hw_tests:
          setattr(hw_test, key, val)

    return cbuildbot_run.BuilderRun(options, site_config, build_config,
                                    parallel.Manager())

  def testGetHWTestStageWithPerModelFilters(self):
    """Verify hwtests are filtered correctly on a per-model basis"""
    extra_argv = ['--hwtest']
    unified_build = self._initConfig('eve-release', extra_argv=extra_argv)
    unified_build.attrs.chrome_version = 'TheChromeVersion'

    test_phase1 = unified_build.config.hw_tests[0]
    test_phase2 = unified_build.config.hw_tests[1]

    model1 = config_lib.ModelTestConfig('model1', 'some_lab_board')
    model2 = config_lib.ModelTestConfig('model2', 'mode11', [test_phase2.suite])

    stage = test_stages.TestPlanStage(unified_build, self.buildstore, 'eve')

    hw_stage = stage._GetHWTestStage(unified_build, self.buildstore, 'eve',
                                     model1, test_phase1)
    self.assertIsNotNone(hw_stage)
    self.assertEqual(hw_stage._board_name, 'some_lab_board')

    hw_stage = stage._GetHWTestStage(unified_build, self.buildstore, 'eve',
                                     model2, test_phase1)
    self.assertIsNone(hw_stage)

    hw_stage = stage._GetHWTestStage(unified_build, self.buildstore, 'eve',
                                     model2, test_phase2)
    self.assertIsNotNone(hw_stage)
