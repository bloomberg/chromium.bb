# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for completion stages."""

from __future__ import print_function

import itertools
import mock
import sys

from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import commands
from chromite.cbuildbot import config_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import prebuilts
from chromite.cbuildbot import results_lib
from chromite.cbuildbot.stages import completion_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import sync_stages_unittest
from chromite.cbuildbot.stages import sync_stages
from chromite.lib import alerts
from chromite.lib import cidb
from chromite.lib import clactions
from chromite.lib import patch as cros_patch
from chromite.lib import patch_unittest


# pylint: disable=protected-access


class ManifestVersionedSyncCompletionStageTest(
    sync_stages_unittest.ManifestVersionedSyncStageTest):
  """Tests the ManifestVersionedSyncCompletion stage."""

  # pylint: disable=abstract-method

  BOT_ID = 'x86-mario-release'

  def testManifestVersionedSyncCompletedSuccess(self):
    """Tests basic ManifestVersionedSyncStageCompleted on success"""
    board_runattrs = self._run.GetBoardRunAttrs('x86-mario')
    board_runattrs.SetParallel('success', True)
    update_status_mock = self.PatchObject(
        manifest_version.BuildSpecsManager, 'UpdateStatus')

    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.sync_stage, success=True)

    stage.Run()
    update_status_mock.assert_called_once_with(
        message=None, success_map={self.BOT_ID: True}, dashboard_url=mock.ANY)

  def testManifestVersionedSyncCompletedFailure(self):
    """Tests basic ManifestVersionedSyncStageCompleted on failure"""
    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.sync_stage, success=False)
    message = 'foo'
    self.PatchObject(stage, 'GetBuildFailureMessage', return_value=message)
    update_status_mock = self.PatchObject(
        manifest_version.BuildSpecsManager, 'UpdateStatus')

    stage.Run()
    update_status_mock.assert_called_once_with(
        message='foo', success_map={self.BOT_ID: False},
        dashboard_url=mock.ANY)

  def testManifestVersionedSyncCompletedIncomplete(self):
    """Tests basic ManifestVersionedSyncStageCompleted on incomplete build."""
    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.sync_stage, success=False)
    stage.Run()

  def testMeaningfulMessage(self):
    """Tests that all essential components are in the message."""
    stage = completion_stages.ManifestVersionedSyncCompletionStage(
        self._run, self.sync_stage, success=False)

    exception = Exception('failed!')
    traceback = results_lib.RecordedTraceback(
        'TacoStage', 'Taco', exception, 'traceback_str')
    self.PatchObject(
        results_lib.Results, 'GetTracebacks', return_value=[traceback])

    msg = stage.GetBuildFailureMessage()
    self.assertTrue(stage._run.config.name in msg.message)
    self.assertTrue(stage._run.ConstructDashboardURL() in msg.message)
    self.assertTrue('TacoStage' in msg.message)
    self.assertTrue(str(exception) in msg.message)

    self.assertTrue('TacoStage' in msg.reason)
    self.assertTrue(str(exception) in msg.reason)

  def testGetBuilderSuccessMap(self):
    """Tests that the builder success map is properly created."""
    board_runattrs = self._run.GetBoardRunAttrs('x86-mario')
    board_runattrs.SetParallel('success', True)
    builder_success_map = completion_stages.GetBuilderSuccessMap(
        self._run, True)
    expected_map = {self.BOT_ID: True}
    self.assertEqual(expected_map, builder_success_map)


class MasterSlaveSyncCompletionStageMockConfigTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests MasterSlaveSyncCompletionStage with ManifestVersionedSyncStage."""
  BOT_ID = 'master'

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_type = constants.PFQ_TYPE

    # Use our mocked out SiteConfig for all tests.
    self.test_config = self._GetTestConfig()
    self._Prepare(site_config=self.test_config)

  def ConstructStage(self):
    sync_stage = sync_stages.ManifestVersionedSyncStage(self._run)
    return completion_stages.MasterSlaveSyncCompletionStage(
        self._run, sync_stage, success=True)

  def _GetTestConfig(self):
    test_config = config_lib.SiteConfig()
    test_config.Add(
        'master',
        config_lib.BuildConfig(),
        boards=[],
        build_type=self.build_type,
        master=True,
        manifest_version=True,
    )
    test_config.Add(
        'test1',
        config_lib.BuildConfig(),
        boards=['x86-generic'],
        manifest_version=True,
        build_type=constants.PFQ_TYPE,
        overlays='public',
        important=False,
        chrome_rev=None,
        branch=False,
        internal=False,
        master=False,
    )
    test_config.Add(
        'test2',
        config_lib.BuildConfig(),
        boards=['x86-generic'],
        manifest_version=False,
        build_type=constants.PFQ_TYPE,
        overlays='public',
        important=True,
        chrome_rev=None,
        branch=False,
        internal=False,
        master=False,
    )
    test_config.Add(
        'test3',
        config_lib.BuildConfig(),
        boards=['x86-generic'],
        manifest_version=True,
        build_type=constants.PFQ_TYPE,
        overlays='both',
        important=True,
        chrome_rev=None,
        branch=False,
        internal=True,
        master=False,
    )
    test_config.Add(
        'test4',
        config_lib.BuildConfig(),
        boards=['x86-generic'],
        manifest_version=True,
        build_type=constants.PFQ_TYPE,
        overlays='both',
        important=True,
        chrome_rev=None,
        branch=True,
        internal=True,
        master=False,
    )
    test_config.Add(
        'test5',
        config_lib.BuildConfig(),
        boards=['x86-generic'],
        manifest_version=True,
        build_type=constants.PFQ_TYPE,
        overlays='public',
        important=True,
        chrome_rev=None,
        branch=False,
        internal=False,
        master=False,
    )
    return test_config

  def testGetSlavesForMaster(self):
    """Tests that we get the slaves for a fake unified master configuration."""
    self.maxDiff = None
    stage = self.ConstructStage()
    p = stage._GetSlaveConfigs()
    self.assertEqual([self.test_config['test3'], self.test_config['test5']], p)


class MasterSlaveSyncCompletionStageTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests MasterSlaveSyncCompletionStage with ManifestVersionedSyncStage."""
  BOT_ID = 'x86-generic-paladin'

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_type = constants.PFQ_TYPE

    self._Prepare()

  def _Prepare(self, bot_id=None, **kwargs):
    super(MasterSlaveSyncCompletionStageTest, self)._Prepare(bot_id, **kwargs)

    self._run.config['manifest_version'] = True
    self._run.config['build_type'] = self.build_type
    self._run.config['master'] = True

  def ConstructStage(self):
    sync_stage = sync_stages.ManifestVersionedSyncStage(self._run)
    return completion_stages.MasterSlaveSyncCompletionStage(
        self._run, sync_stage, success=True)

  def testIsFailureFatal(self):
    """Tests the correctness of the _IsFailureFatal method"""
    stage = self.ConstructStage()

    # Test behavior when there are no sanity check builders
    self.assertFalse(stage._IsFailureFatal(set(), set(), set()))
    self.assertTrue(stage._IsFailureFatal(set(['test3']), set(), set()))
    self.assertTrue(stage._IsFailureFatal(set(), set(['test5']), set()))
    self.assertTrue(stage._IsFailureFatal(set(), set(), set(['test1'])))

    # Test behavior where there is a sanity check builder
    stage._run.config.sanity_check_slaves = ['sanity']
    self.assertTrue(stage._IsFailureFatal(set(['test5']), set(['sanity']),
                                          set()))
    self.assertFalse(stage._IsFailureFatal(set(), set(['sanity']), set()))
    self.assertTrue(stage._IsFailureFatal(set(), set(['sanity']),
                                          set(['test1'])))
    self.assertFalse(stage._IsFailureFatal(set(), set(),
                                           set(['sanity'])))

  def testAnnotateFailingBuilders(self):
    """Tests that _AnnotateFailingBuilders is free of syntax errors."""
    stage = self.ConstructStage()

    failing = {'a'}
    inflight = {}
    failed_msg = failures_lib.BuildFailureMessage(
        'message', [], True, 'reason', 'bot')
    status = manifest_version.BuilderStatus('failed', failed_msg, 'url')

    statuses = {'a' : status}
    no_stat = set()
    stage._AnnotateFailingBuilders(failing, inflight, no_stat, statuses)

  def testExceptionHandler(self):
    """Verify _HandleStageException is sane."""
    stage = self.ConstructStage()
    e = ValueError('foo')
    try:
      raise e
    except ValueError:
      ret = stage._HandleStageException(sys.exc_info())
      self.assertTrue(isinstance(ret, tuple))
      self.assertEqual(len(ret), 3)
      self.assertEqual(ret[0], e)


class MasterSlaveSyncCompletionStageTestWithLKGMSync(
    MasterSlaveSyncCompletionStageTest):
  """Tests the MasterSlaveSyncCompletionStage with MasterSlaveLKGMSyncStage."""
  BOT_ID = 'x86-generic-paladin'

  def ConstructStage(self):
    sync_stage = sync_stages.MasterSlaveLKGMSyncStage(self._run)
    return completion_stages.MasterSlaveSyncCompletionStage(
        self._run, sync_stage, success=True)


class CanaryCompletionStageTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests how canary master handles failures in CanaryCompletionStage."""
  BOT_ID = 'master-release'

  def _Prepare(self, bot_id=BOT_ID, **kwargs):
    super(CanaryCompletionStageTest, self)._Prepare(bot_id, **kwargs)

  def setUp(self):
    self.build_type = constants.CANARY_TYPE
    self._Prepare()

  def ConstructStage(self):
    """Returns a CanaryCompletionStage object."""
    sync_stage = sync_stages.ManifestVersionedSyncStage(self._run)
    return completion_stages.CanaryCompletionStage(
        self._run, sync_stage, success=True)

  def testComposeTreeStatusMessage(self):
    """Tests that the status message is constructed as expected."""
    failing = ['foo1', 'foo2', 'foo3', 'foo4', 'foo5']
    inflight = ['bar']
    no_stat = []
    stage = self.ConstructStage()
    self.assertEqual(
        stage._ComposeTreeStatusMessage(failing, inflight, no_stat),
        'bar timed out; foo1,foo2 and 3 others failed')


class BaseCommitQueueCompletionStageTest(
    generic_stages_unittest.AbstractStageTestCase,
    patch_unittest.MockPatchBase):
  """Tests how CQ handles changes in CommitQueueCompletionStage."""

  def setUp(self):
    self.build_type = constants.PFQ_TYPE
    self._Prepare()

    self.partial_submit_changes = ['C', 'D']
    self.other_changes = ['A', 'B']
    self.changes = self.other_changes + self.partial_submit_changes
    self.tot_sanity_mock = self.PatchObject(
        completion_stages.CommitQueueCompletionStage,
        '_ToTSanity',
        return_value=True)

    self.alert_email_mock = self.PatchObject(alerts, 'SendEmail')
    self.PatchObject(cbuildbot_run._BuilderRunBase,
                     'InEmailReportingEnvironment', return_value=True)
    self.PatchObject(completion_stages.MasterSlaveSyncCompletionStage,
                     'HandleFailure')
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetFailedMessages')
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetSlaveMappingAndCLActions',
                     return_value=(dict(), []))
    self.PatchObject(clactions, 'GetRelevantChangesForBuilds')

  # pylint: disable=W0221
  def ConstructStage(self, tree_was_open=True):
    """Returns a CommitQueueCompletionStage object.

    Args:
      tree_was_open: If not true, tree was not open when we acquired changes.
    """
    sync_stage = sync_stages.CommitQueueSyncStage(self._run)
    sync_stage.pool = mock.MagicMock()
    sync_stage.pool.applied = self.changes
    sync_stage.pool.tree_was_open = tree_was_open

    sync_stage.pool.handle_failure_mock = self.PatchObject(
        sync_stage.pool, 'HandleValidationFailure')
    sync_stage.pool.handle_timeout_mock = self.PatchObject(
        sync_stage.pool, 'HandleValidationTimeout')
    return completion_stages.CommitQueueCompletionStage(
        self._run, sync_stage, success=True)

  def VerifyStage(self, failing, inflight, handle_failure=True,
                  handle_timeout=False, sane_tot=True, submit_partial=False,
                  alert=False, stage=None, all_slaves=None, slave_stages=None,
                  do_submit_partial=True, build_passed=False):
    """Runs and Verifies PerformStage.

    Args:
      failing: The names of the builders that failed.
      inflight: The names of the buiders that timed out.
      handle_failure: If True, calls HandleValidationFailure.
      handle_timeout: If True, calls HandleValidationTimeout.
      sane_tot: If not true, assumes TOT is not sane.
      submit_partial: If True, submit partial pool will submit some changes.
      alert: If True, sends out an alert email for infra failures.
      stage: If set, use this constructed stage, otherwise create own.
      all_slaves: Optional set of all slave configs.
      slave_stages: Optional list of slave stages.
      do_submit_partial: If True, assert that there was no call to
                         SubmitPartialPool.
      build_passed: Whether the build passed or failed.
    """
    if not stage:
      stage = self.ConstructStage()

    # Setup the stage to look at the specified configs.
    all_slaves = list(all_slaves or set(failing + inflight))
    configs = [config_lib.BuildConfig(name=x) for x in all_slaves]
    self.PatchObject(stage, '_GetSlaveConfigs', return_value=configs)

    # Setup builder statuses.
    stage._run.attrs.manifest_manager = mock.MagicMock()
    statuses = {}
    for x in failing:
      statuses[x] = manifest_version.BuilderStatus(
          constants.BUILDER_STATUS_FAILED, message=None)
    for x in inflight:
      statuses[x] = manifest_version.BuilderStatus(
          constants.BUILDER_STATUS_INFLIGHT, message=None)
    if self._run.config.master:
      self.PatchObject(stage._run.attrs.manifest_manager, 'GetBuildersStatus',
                       return_value=statuses)
    else:
      self.PatchObject(stage, '_GetLocalBuildStatus', return_value=statuses)

    # Setup DB and provide list of slave stages.
    mock_cidb = mock.MagicMock()
    cidb.CIDBConnectionFactory.SetupMockCidb(mock_cidb)
    if slave_stages is None:
      slave_stages = []
      critical_stages = (
          completion_stages.CommitQueueCompletionStage._CRITICAL_STAGES)
      for stage_name, slave in itertools.product(critical_stages, all_slaves):
        slave_stages.append({'name': stage_name,
                             'build_config': slave,
                             'status': constants.BUILDER_STATUS_PASSED})
    self.PatchObject(mock_cidb, 'GetSlaveStages', return_value=slave_stages)


    # Set up SubmitPartialPool to provide a list of changes to look at.
    if submit_partial:
      spmock = self.PatchObject(stage.sync_stage.pool, 'SubmitPartialPool',
                                return_value=self.other_changes)
      handlefailure_changes = self.other_changes
    else:
      spmock = self.PatchObject(stage.sync_stage.pool, 'SubmitPartialPool',
                                return_value=self.changes)
      handlefailure_changes = self.changes

    # Track whether 'HandleSuccess' is called.
    success_mock = self.PatchObject(stage, 'HandleSuccess')

    # Actually run the stage.
    if build_passed:
      stage.PerformStage()
    else:
      with self.assertRaises(completion_stages.ImportantBuilderFailedException):
        stage.PerformStage()

    # Verify the calls.
    self.assertEqual(success_mock.called, build_passed)

    if not build_passed and self._run.config.master:
      self.tot_sanity_mock.assert_called_once_with(mock.ANY, mock.ANY)

      if alert:
        self.alert_email_mock.called_once_with(
            mock.ANY, mock.ANY, mock.ANY, mock.ANY)

      self.assertEqual(do_submit_partial, spmock.called)

      if handle_failure:
        stage.sync_stage.pool.handle_failure_mock.assert_called_once_with(
            mock.ANY, no_stat=set([]), sanity=sane_tot,
            changes=handlefailure_changes)

      if handle_timeout:
        stage.sync_stage.pool.handle_timeout_mock.assert_called_once_with(
            sanity=mock.ANY, changes=self.changes)


# pylint: disable=too-many-ancestors
class SlaveCommitQueueCompletionStageTest(BaseCommitQueueCompletionStageTest):
  """Tests how CQ a slave handles changes in CommitQueueCompletionStage."""
  BOT_ID = 'x86-mario-paladin'

  def testSuccess(self):
    """Test the slave succeeding."""
    self.VerifyStage([], [], build_passed=True)

  def testFail(self):
    """Test the slave failing."""
    self.VerifyStage(['foo'], [], build_passed=False)

  def testTimeout(self):
    """Test the slave timing out."""
    self.VerifyStage([], ['foo'], build_passed=False)


class MasterCommitQueueCompletionStageTest(BaseCommitQueueCompletionStageTest):
  """Tests how CQ master handles changes in CommitQueueCompletionStage."""
  BOT_ID = 'master-paladin'

  def _Prepare(self, bot_id=BOT_ID, **kwargs):
    super(MasterCommitQueueCompletionStageTest, self)._Prepare(bot_id, **kwargs)
    self.assertTrue(self._run.config['master'])

  def testNoInflightBuildersWithInfraFail(self):
    """Test case where there are no inflight builders but are infra failures."""
    failing = ['foo']
    inflight = []

    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=['msg'])
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildersWithNoneMessages', return_value=[])
    # An alert is sent, since there are infra failures.
    self.VerifyStage(failing, inflight, submit_partial=True, alert=True)

  def testMissingCriticalStage(self):
    """Test case where a slave failed to run a critical stage."""
    self.VerifyStage(['foo'], [], slave_stages=[],
                     do_submit_partial=False)

  def testFailedCriticalStage(self):
    """Test case where a slave failed a critical stage."""
    fake_stages = [{'name': 'CommitQueueSync', 'build_config': 'foo',
                    'status': constants.BUILDER_STATUS_FAILED}]
    self.VerifyStage(['foo'], [],
                     slave_stages=fake_stages, do_submit_partial=False)

  def testMissingCriticalStageOnSanitySlave(self):
    """Test case where a sanity slave failed to run a critical stage."""
    stage = self.ConstructStage()
    fake_stages = [{'name': 'CommitQueueSync', 'build_config': 'foo',
                    'status': constants.BUILDER_STATUS_PASSED}]
    stage._run.config.sanity_check_slaves = ['sanity']
    self.VerifyStage(['sanity', 'foo'], [], slave_stages=fake_stages,
                     do_submit_partial=True, stage=stage)

  def testMissingCriticalStageOnTimedOutSanitySlave(self):
    """Test case where a sanity slave failed to run a critical stage."""
    stage = self.ConstructStage()
    fake_stages = [{'name': 'CommitQueueSync', 'build_config': 'foo',
                    'status': constants.BUILDER_STATUS_PASSED}]
    stage._run.config.sanity_check_slaves = ['sanity']
    self.VerifyStage(['foo'], ['sanity'], slave_stages=fake_stages,
                     do_submit_partial=True, stage=stage,
                     handle_failure=False, handle_timeout=True)

  def testNoInflightBuildersWithNoneFailureMessages(self):
    """Test case where failed builders reported NoneType messages."""
    failing = ['foo']
    inflight = []

    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=[])
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildersWithNoneMessages', return_value=['foo'])
    # An alert is sent, since NonType messages are considered infra failures.
    self.VerifyStage(failing, inflight, submit_partial=True, alert=True)

  def testWithInflightBuildersNoInfraFail(self):
    """Tests that we don't submit partial pool on non-empty inflight."""
    failing = ['foo', 'bar']
    inflight = ['inflight']

    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=[])
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildersWithNoneMessages', return_value=[])

    # An alert is sent, since we have an inflight build still.
    self.VerifyStage(failing, inflight, handle_failure=False,
                     handle_timeout=True, alert=True)

  def testSanityFailed(self):
    """Test case where the sanity builder failed."""
    stage = self.ConstructStage()
    stage._run.config.sanity_check_slaves = ['sanity']
    self.VerifyStage(['sanity'], [], build_passed=True)

  def testSanityTimeout(self):
    """Test case where the sanity builder timed out."""
    stage = self.ConstructStage()
    stage._run.config.sanity_check_slaves = ['sanity']
    self.VerifyStage([], ['sanity'], build_passed=True)

  def testGetRelevantChangesForSlave(self):
    """Tests the logic of GetRelevantChangesForSlaves()."""
    change_set1 = set(self.GetPatches(how_many=2))
    change_set2 = set(self.GetPatches(how_many=3))
    changes = set.union(change_set1, change_set2)
    no_stat = ['no_stat-paladin']
    config_map = {'123': 'foo-paladin',
                  '124': 'bar-paladin',
                  '125': 'no_stat-paladin'}
    changes_by_build_id = {'123': change_set1,
                           '124': change_set2}
    # If a slave did not report status (no_stat), assume all changes
    # are relevant.
    expected = {'foo-paladin': change_set1,
                'bar-paladin': change_set2,
                'no_stat-paladin': changes}
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetSlaveMappingAndCLActions',
                     return_value=(config_map, []))
    self.PatchObject(clactions, 'GetRelevantChangesForBuilds',
                     return_value=changes_by_build_id)

    stage = self.ConstructStage()
    results = stage.GetRelevantChangesForSlaves(changes, no_stat)
    self.assertEqual(results, expected)

  def testWithExponentialFallbackApplied(self):
    """Tests that we don't treat TOT as sane when it isn't."""
    failing = ['foo', 'bar']
    inflight = ['inflight']
    stage = self.ConstructStage(tree_was_open=False)
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=[])
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildersWithNoneMessages', return_value=['foo'])

    # An alert is sent, since we have an inflight build still.
    self.VerifyStage(failing, inflight, handle_failure=False,
                     handle_timeout=False, sane_tot=False, alert=True,
                     stage=stage)

  def testGetIrrelevantChanges(self):
    """Tests the logic of GetIrrelevantChanges()."""
    change_dict_1 = {
        cros_patch.ATTR_PROJECT_URL: 'https://host/chromite/tacos',
        cros_patch.ATTR_PROJECT: 'chromite/tacos',
        cros_patch.ATTR_REF: 'refs/changes/11/12345/4',
        cros_patch.ATTR_BRANCH: 'master',
        cros_patch.ATTR_REMOTE: 'cros-internal',
        cros_patch.ATTR_COMMIT: '7181e4b5e182b6f7d68461b04253de095bad74f9',
        cros_patch.ATTR_CHANGE_ID: 'I47ea30385af60ae4cc2acc5d1a283a46423bc6e1',
        cros_patch.ATTR_GERRIT_NUMBER: '12345',
        cros_patch.ATTR_PATCH_NUMBER: '4',
        cros_patch.ATTR_OWNER_EMAIL: 'foo@chromium.org',
        cros_patch.ATTR_FAIL_COUNT: 1,
        cros_patch.ATTR_PASS_COUNT: 1,
        cros_patch.ATTR_TOTAL_FAIL_COUNT: 3}
    change_dict_2 = {
        cros_patch.ATTR_PROJECT_URL: 'https://host/chromite/foo',
        cros_patch.ATTR_PROJECT: 'chromite/foo',
        cros_patch.ATTR_REF: 'refs/changes/11/12344/3',
        cros_patch.ATTR_BRANCH: 'master',
        cros_patch.ATTR_REMOTE: 'cros-internal',
        cros_patch.ATTR_COMMIT: 'cf23df2207d99a74fbe169e3eba035e633b65d94',
        cros_patch.ATTR_CHANGE_ID: 'Iab9bf08b9b9bd4f72721cfc36e843ed302aca11a',
        cros_patch.ATTR_GERRIT_NUMBER: '12344',
        cros_patch.ATTR_PATCH_NUMBER: '3',
        cros_patch.ATTR_OWNER_EMAIL: 'foo@chromium.org',
        cros_patch.ATTR_FAIL_COUNT: 0,
        cros_patch.ATTR_PASS_COUNT: 0,
        cros_patch.ATTR_TOTAL_FAIL_COUNT: 1}
    change_1 = cros_patch.GerritFetchOnlyPatch.FromAttrDict(change_dict_1)
    change_2 = cros_patch.GerritFetchOnlyPatch.FromAttrDict(change_dict_2)

    board_metadata_1 = {
        'board-1': {'info':'foo', 'irrelevant_changes': [change_dict_1,
                                                         change_dict_2]},
        'board-2': {'info':'foo', 'irrelevant_changes': [change_dict_1]}
    }
    board_metadata_2 = {
        'board-1': {'info':'foo', 'irrelevant_changes': [change_dict_1]},
        'board-2': {'info':'foo', 'irrelevant_changes': [change_dict_2]}
    }
    board_metadata_3 = {
        'board-1': {'info':'foo', 'irrelevant_changes': [change_dict_1,
                                                         change_dict_2]},
        'board-2': {'info':'foo', 'irrelevant_changes': []}
    }
    board_metadata_4 = {
        'board-1': {'info':'foo', 'irrelevant_changes': [change_dict_1,
                                                         change_dict_2]},
        'board-2': {'info':'foo'}
    }
    board_metadata_5 = {}
    board_metadata_6 = {
        'board-1': {'info':'foo', 'irrelevant_changes': [change_dict_1,
                                                         change_dict_2]},
    }
    stage = self.ConstructStage()
    self.assertEqual(stage.GetIrrelevantChanges(board_metadata_1), {change_1})
    self.assertEqual(stage.GetIrrelevantChanges(board_metadata_2), set())
    self.assertEqual(stage.GetIrrelevantChanges(board_metadata_3), set())
    self.assertEqual(stage.GetIrrelevantChanges(board_metadata_4), set())
    self.assertEqual(stage.GetIrrelevantChanges(board_metadata_5), set())
    self.assertEqual(stage.GetIrrelevantChanges(board_metadata_6), {change_1,
                                                                    change_2})

  def testGetSubsysResultForSlaves(self):
    """Tests for the GetSubsysResultForSlaves."""
    def get_dict(build_config, message_type, message_subtype, message_value):
      return {'build_config': build_config,
              'message_type': message_type,
              'message_subtype': message_subtype,
              'message_value': message_value}

    slave_msgs = [get_dict('config_1', constants.SUBSYSTEMS,
                           constants.SUBSYSTEM_PASS, 'a'),
                  get_dict('config_1', constants.SUBSYSTEMS,
                           constants.SUBSYSTEM_PASS, 'b'),
                  get_dict('config_1', constants.SUBSYSTEMS,
                           constants.SUBSYSTEM_FAIL, 'c'),
                  get_dict('config_2', constants.SUBSYSTEMS,
                           constants.SUBSYSTEM_UNUSED, None),
                  get_dict('config_3', constants.SUBSYSTEMS,
                           constants.SUBSYSTEM_PASS, 'a'),
                  get_dict('config_3', constants.SUBSYSTEMS,
                           constants.SUBSYSTEM_PASS, 'e'),]
    # Setup DB and provide list of slave build messages.
    mock_cidb = mock.MagicMock()
    cidb.CIDBConnectionFactory.SetupMockCidb(mock_cidb)
    self.PatchObject(mock_cidb, 'GetSlaveBuildMessages',
                     return_value=slave_msgs)

    expect_result = {
        'config_1': {'pass_subsystems':set(['a', 'b']),
                     'fail_subsystems':set(['c'])},
        'config_2': {},
        'config_3': {'pass_subsystems':set(['a', 'e'])}}
    stage = self.ConstructStage()
    self.assertEqual(stage.GetSubsysResultForSlaves(), expect_result)

class PublishUprevChangesStageTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests for the PublishUprevChanges stage."""
  BOT_ID = 'master-chromium-pfq'

  def _Prepare(self, bot_id=None, **kwargs):
    super(PublishUprevChangesStageTest, self)._Prepare(bot_id, **kwargs)

    self._run.config['manifest_version'] = True
    self._run.config['build_type'] = self.build_type
    self._run.config['master'] = True

  def setUp(self):
    self.PatchObject(completion_stages.PublishUprevChangesStage,
                     '_GetPortageEnvVar')
    self.PatchObject(completion_stages.PublishUprevChangesStage,
                     '_ExtractOverlays', return_value=[['foo'], ['bar']])
    self.PatchObject(prebuilts.BinhostConfWriter, 'Perform')
    self.push_mock = self.PatchObject(commands, 'UprevPush')
    self.build_type = constants.PFQ_TYPE

    self._Prepare()

  def ConstructStage(self):
    return completion_stages.PublishUprevChangesStage(self._run, success=True)

  def testPush(self):
    """Test values for PublishUprevChanges."""
    self._Prepare(extra_config={'build_type': constants.BUILD_FROM_SOURCE_TYPE,
                                'push_overlays': constants.PUBLIC_OVERLAYS,
                                'master': True},
                  extra_cmd_args=['--chrome_rev', constants.CHROME_REV_TOT])
    self._run.options.prebuilts = True
    self.RunStage()
    self.push_mock.assert_called_once_with(self.build_root, ['bar'], False,
                                           staging_branch=None)

  def testCheckSlaveUploadPrebuiltsTest(self):
    """Tests for CheckSlaveUploadPrebuiltsTest."""
    stage = self.ConstructStage()
    stage._build_stage_id = 'test_build_stage_id'

    build_id = 'test_master_build_id'
    mock_cidb = mock.MagicMock()
    cidb.CIDBConnectionFactory.SetupMockCidb(mock_cidb)

    stage_name = 'UploadPrebuilts'

    slave_a = 'slave_a'
    slave_b = 'slave_b'
    slave_c = 'slave_c'

    slave_configs_a = [{'name': slave_a},
                       {'name': slave_b}]
    slave_stages_a = [{'name': stage_name,
                       'build_config': slave_a,
                       'status': constants.BUILDER_STATUS_PASSED},
                      {'name': stage_name,
                       'build_config': slave_b,
                       'status': constants.BUILDER_STATUS_PASSED}]

    self.PatchObject(completion_stages.PublishUprevChangesStage,
                     '_GetSlaveConfigs', return_value=slave_configs_a)
    self.PatchObject(mock_cidb, 'GetSlaveStages',
                     return_value=slave_stages_a)

    # All important slaves are covered
    self.assertTrue(stage.CheckSlaveUploadPrebuiltsTest(
        mock_cidb, build_id))

    slave_stages_b = [{'name': stage_name,
                       'build_config': slave_a,
                       'status': constants.BUILDER_STATUS_FAILED},
                      {'name': stage_name,
                       'build_config': slave_b,
                       'status': constants.BUILDER_STATUS_PASSED}]
    self.PatchObject(completion_stages.PublishUprevChangesStage,
                     '_GetSlaveConfigs', return_value=slave_configs_a)
    self.PatchObject(mock_cidb, 'GetSlaveStages',
                     return_value=slave_stages_b)

    # Slave_a didn't pass the stage
    self.assertFalse(stage.CheckSlaveUploadPrebuiltsTest(
        mock_cidb, build_id))

    slave_configs_b = [{'name': slave_a},
                       {'name': slave_b},
                       {'name': slave_c}]
    self.PatchObject(completion_stages.PublishUprevChangesStage,
                     '_GetSlaveConfigs', return_value=slave_configs_b)
    self.PatchObject(mock_cidb, 'GetSlaveStages',
                     return_value=slave_stages_a)

    # No stage information for slave_c
    self.assertFalse(stage.CheckSlaveUploadPrebuiltsTest(
        mock_cidb, build_id))
