#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for test stages."""

from __future__ import print_function

import mox
import os
import sys

sys.path.insert(0, os.path.abspath('%s/../../..' % os.path.dirname(__file__)))
from chromite.cbuildbot import cbuildbot_config as config
from chromite.cbuildbot import commands
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import lab_status
from chromite.cbuildbot.cbuildbot_unittest import BuilderRunMock
from chromite.cbuildbot.stages import artifact_stages
from chromite.cbuildbot.stages import test_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.lib import cgroups
from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import osutils
from chromite.lib import timeout_util


# pylint: disable=R0901
class VMTestStageTest(generic_stages_unittest.AbstractStageTest):
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
    self.StartPatcher(BuilderRunMock())
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


class UnitTestStageTest(generic_stages_unittest.AbstractStageTest):
  """Tests for the UnitTest stage."""

  BOT_ID = 'x86-generic-full'

  def setUp(self):
    self.mox.StubOutWithMock(commands, 'RunUnitTests')
    self.mox.StubOutWithMock(commands, 'TestAuZip')

    self._Prepare()

  def ConstructStage(self):
    return test_stages.UnitTestStage(self._run, self._current_board)

  def testQuickTests(self):
    self.mox.StubOutWithMock(os.path, 'exists')
    self._run.config['quick_unit'] = True
    commands.RunUnitTests(self.build_root, self._current_board, full=False,
                          blacklist=[], extra_env=mox.IgnoreArg())
    image_dir = os.path.join(self.build_root,
                             'src/build/images/x86-generic/latest-cbuildbot')
    os.path.exists(os.path.join(image_dir,
                                'au-generator.zip')).AndReturn(True)
    commands.TestAuZip(self.build_root, image_dir)
    self.mox.ReplayAll()

    self.RunStage()
    self.mox.VerifyAll()

  def testQuickTestsAuGeneratorZipMissing(self):
    self.mox.StubOutWithMock(os.path, 'exists')
    self._run.config['quick_unit'] = True
    commands.RunUnitTests(self.build_root, self._current_board, full=False,
                          blacklist=[], extra_env=mox.IgnoreArg())
    image_dir = os.path.join(self.build_root,
                             'src/build/images/x86-generic/latest-cbuildbot')
    os.path.exists(os.path.join(image_dir,
                                'au-generator.zip')).AndReturn(False)
    self.mox.ReplayAll()

    self.RunStage()
    self.mox.VerifyAll()

  def testFullTests(self):
    """Tests if full unit and cros_au_test_harness tests are run correctly."""
    self.mox.StubOutWithMock(os.path, 'exists')
    self._run.config['quick_unit'] = False
    commands.RunUnitTests(self.build_root, self._current_board, full=True,
                          blacklist=[], extra_env=mox.IgnoreArg())
    image_dir = os.path.join(self.build_root,
                             'src/build/images/x86-generic/latest-cbuildbot')
    os.path.exists(os.path.join(image_dir,
                                'au-generator.zip')).AndReturn(True)
    commands.TestAuZip(self.build_root, image_dir)
    self.mox.ReplayAll()

    self.RunStage()
    self.mox.VerifyAll()


class HWTestStageTest(generic_stages_unittest.AbstractStageTest):
  """Tests for the HWTest stage."""

  BOT_ID = 'x86-mario-release'
  VERSION = 'R36-5760.0.0'
  RELEASE_TAG = ''

  def setUp(self):
    self.StartPatcher(BuilderRunMock())

    self.mox.StubOutWithMock(lab_status, 'CheckLabStatus')
    self.mox.StubOutWithMock(commands, 'HaveCQHWTestsBeenAborted')
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    self.mox.StubOutWithMock(cros_build_lib, 'PrintBuildbotStepWarnings')
    self.mox.StubOutWithMock(cros_build_lib, 'PrintBuildbotStepFailure')
    self.mox.StubOutWithMock(cros_build_lib, 'Warning')
    self.mox.StubOutWithMock(cros_build_lib, 'Error')

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
    return test_stages.HWTestStage(
        self._run, self._current_board, self.suite_config)

  def _RunHWTestSuite(self, debug=False, returncode=0, fails=False,
                      timeout=False):
    """Pretend to run the HWTest suite to assist with tests.

    Args:
      debug: Whether the HWTest suite should be run in debug mode.
      returncode: The return value of the HWTest command.
      fails: Whether the command as a whole should fail.
      timeout: Whether the the hw tests should time out.
    """
    if config.IsCQType(self._run.config.build_type):
      version = self._run.GetVersion()
      for _ in range(1 + int(returncode != 0)):
        commands.HaveCQHWTestsBeenAborted(version).AndReturn(False)

    lab_status.CheckLabStatus(mox.IgnoreArg())

    if not debug:
      m = cros_build_lib.RunCommand(mox.IgnoreArg(), error_code_ok=True)
      result = cros_build_lib.CommandResult(cmd='run_hw_tests',
                                            returncode=returncode)
      m.AndReturn(result)

      # Raise an exception if the user wanted the command to fail.
      if timeout:
        m.AndRaise(timeout_util.TimeoutError('Timed out'))

      # Make sure failures are logged correctly.
      if timeout or returncode != 0:
        if fails:
          cros_build_lib.PrintBuildbotStepFailure()
          cros_build_lib.Error(mox.IgnoreArg())
        else:
          cros_build_lib.PrintBuildbotStepWarnings()
          cros_build_lib.Warning(mox.IgnoreArg())

    self.mox.ReplayAll()
    if fails:
      self.assertRaises(failures_lib.StepFailure, self.RunStage)
    else:
      self.RunStage()
    self.mox.VerifyAll()

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

  def testHandleWarningCodeForCQ(self):
    """Tests that we pass CQ on WARNING."""
    self._Prepare('x86-alex-paladin')
    self._RunHWTestSuite(returncode=2, fails=False)

  def testHandleWarningCodeForPFQ(self):
    """Tests that we pass PFQ on WARNING."""
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(returncode=2, fails=False)

  def testHandleWarningCodeForCanary(self):
    """Tests that we pass canary on WARNING."""
    self._Prepare('x86-alex-release')
    self._RunHWTestSuite(returncode=2, fails=False)

  def testHandleInfraErrorCodeForCQ(self):
    """Tests that we fail CQ on INFRA_FAILURE."""
    self._Prepare('x86-alex-paladin')
    self._RunHWTestSuite(returncode=3, fails=True)

  def testHandleInfraErrorCodeForPFQ(self):
    """Tests that we fail PFQ on INFRA_FAILURE."""
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(returncode=3, fails=True)

  def testHandleInfraErrorCodeForCanary(self):
    """Tests that we fail canary on INFRA_FAILURE."""
    self._Prepare('x86-alex-release')
    self._RunHWTestSuite(returncode=3, fails=True)

  def testWithSuiteWithFatalFailure(self):
    """Tests that we fail on ERROR."""
    self._RunHWTestSuite(returncode=1, fails=True)

  def testWithSuiteWithFatalFailureWarnFlag(self):
    """Tests that we don't fail if HWTestConfig warn_only is True."""
    self._Prepare('x86-alex-release', warn_only=True)
    self._RunHWTestSuite(returncode=1, fails=False)

  def testReturnTimeoutForCanary(self):
    """Tests that we fail canary on SUITE_TIMEOUT."""
    self._Prepare('x86-alex-release')
    self._RunHWTestSuite(returncode=4, fails=True)

  def testReturnTimeoutForCQ(self):
    """Tests that we fail CQ on SUITE_TIMEOUT."""
    self._Prepare('x86-alex-paladin')
    self._RunHWTestSuite(returncode=4, fails=True)

  def testReturnTimeoutForPFQ(self):
    """Tests that we fail PFQ on SUITE_TIMEOUT."""
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(returncode=4, fails=True)

  def testReturnBoardNotAvailableForCanary(self):
    """Tests that we pass canary on BOARD_NOT_AVAILABLE."""
    self._Prepare('x86-alex-release')
    self._RunHWTestSuite(returncode=5, fails=False)

  def testReturnBoardNotAvailableForCQ(self):
    """Tests that we fail CQ on BOARD_NOT_AVAILABLE."""
    self._Prepare('x86-alex-paladin')
    self._RunHWTestSuite(returncode=5, fails=True)

  def testReturnBoardNotAvailableForPFQ(self):
    """Tests that we fail PFQ on BOARD_NOT_AVAILABLE."""
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(returncode=5, fails=True)

  def testRaiseTimeoutForCanary(self):
    """Canary should fail if timeout exception is raised."""
    self._Prepare('x86-alex-release')
    self._RunHWTestSuite(timeout=True, fails=True)

  def testRaiseTimeoutForCQ(self):
    """CQ should fail if timeout exception is raised."""
    self._Prepare('x86-alex-paladin')
    self._RunHWTestSuite(timeout=True, fails=True)

  def testRaiseTimeoutForPFQ(self):
    """PFQ should fail if timeout exception is raised."""
    self._Prepare('falco-chrome-pfq')
    self._RunHWTestSuite(timeout=True, fails=True)

  def testHandleLabDownAsFatal(self):
    """Test that the stage fails when lab is down."""
    self._Prepare('lumpy-paladin')
    commands.HaveCQHWTestsBeenAborted(mox.IgnoreArg()).AndReturn(False)
    check_lab = lab_status.CheckLabStatus(mox.IgnoreArg())
    check_lab.AndRaise(lab_status.LabIsDownException('Lab is not up.'))
    commands.HaveCQHWTestsBeenAborted(mox.IgnoreArg()).AndReturn(False)
    cros_build_lib.PrintBuildbotStepFailure()
    cros_build_lib.Error(mox.IgnoreArg())
    self.mox.ReplayAll()
    self.assertRaises(failures_lib.StepFailure, self.RunStage)
    self.mox.VerifyAll()

  def testCheckAbortedFailedForCQ(self):
    """Test that when unable to check, treat it as a failure."""
    self._Prepare('lumpy-paladin')
    commands.HaveCQHWTestsBeenAborted(mox.IgnoreArg()).AndRaise(
        Exception('error'))
    commands.HaveCQHWTestsBeenAborted(mox.IgnoreArg()).AndRaise(
        Exception('error'))
    cros_build_lib.PrintBuildbotStepFailure()
    cros_build_lib.Error(mox.IgnoreArg())
    self.mox.ReplayAll()
    self.assertRaises(failures_lib.StepFailure, self.RunStage)
    self.mox.VerifyAll()

  def testBranchedBuildExtendsTimeouts(self):
    """Tests that we run with an extended timeout on a branched build."""
    cmd_args = ['--branch', 'notTot', '-r', self.build_root,
                '--remote-trybot', '--hwtest']
    self._Prepare('x86-alex-release', cmd_args=cmd_args)
    self._RunHWTestSuite()
    self.assertEqual(self.suite_config.timeout,
                     config.HWTestConfig.BRANCHED_HW_TEST_TIMEOUT)
    self.assertEqual(self.suite_config.priority,
                     constants.HWTEST_DEFAULT_PRIORITY)


class AUTestStageTest(generic_stages_unittest.AbstractStageTest,
                      cros_build_lib_unittest.RunCommandTestCase):
  """Test only custom methods in AUTestStageTest."""
  BOT_ID = 'x86-mario-release'
  RELEASE_TAG = '0.0.1'

  def setUp(self):
    self.StartPatcher(BuilderRunMock())
    self.PatchObject(commands, 'ArchiveFile', autospec=True,
                     return_value='foo.txt')
    self.PatchObject(commands, 'HaveCQHWTestsBeenAborted', autospec=True,
                     return_value=False)
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


class ImageTestStageTest(generic_stages_unittest.AbstractStageTest,
                         cros_build_lib_unittest.RunCommandTestCase):
  """Test image test stage."""

  BOT_ID = 'x86-mario-release'
  RELEASE_TAG = 'ToT.0.0'

  def setUp(self):
    self._test_root = os.path.join(self.build_root, 'tmp/results_dir')
    self.PatchObject(commands, 'CreateTestRoot', autospec=True,
                     return_value='/tmp/results_dir')
    self.PatchObject(git, 'ReinterpretPathForChroot',
                     side_effect=lambda x: x)
    self.StartPatcher(BuilderRunMock())
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
        cros_build_lib.ToChrootPath(os.path.join(self._test_root,
                                                 'image_test_results')),
        cros_build_lib.ToChrootPath(stage.GetImageDirSymlink()),
    ]
    self.assertCommandContains(cmd)


if __name__ == '__main__':
  cros_test_lib.main()
