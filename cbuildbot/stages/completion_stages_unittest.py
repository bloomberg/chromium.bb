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
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import prebuilts
from chromite.cbuildbot import relevant_changes
from chromite.cbuildbot import tree_status
from chromite.cbuildbot.stages import completion_stages
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import sync_stages_unittest
from chromite.cbuildbot.stages import sync_stages
from chromite.lib import alerts
from chromite.lib import auth
from chromite.lib import buildbucket_lib
from chromite.lib import builder_status_lib
from chromite.lib import cidb
from chromite.lib import clactions
from chromite.lib import cros_logging as logging
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import failures_lib
from chromite.lib import fake_cidb
from chromite.lib import hwtest_results
from chromite.lib import results_lib
from chromite.lib import patch_unittest
from chromite.lib import timeout_util


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
    get_msg_from_cidb_mock = self.PatchObject(
        stage, 'GetBuildFailureMessageFromCIDB', return_value=message)
    update_status_mock = self.PatchObject(
        manifest_version.BuildSpecsManager, 'UpdateStatus')

    stage.Run()
    update_status_mock.assert_called_once_with(
        message='foo', success_map={self.BOT_ID: False},
        dashboard_url=mock.ANY)
    get_msg_from_cidb_mock.assert_called_once_with()

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
        slave_configs=['test3', 'test5'],
        manifest_version=True,
        active_waterfall=constants.WATERFALL_INTERNAL,
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
        active_waterfall=constants.WATERFALL_INTERNAL,
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
        active_waterfall=constants.WATERFALL_INTERNAL,
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
        active_waterfall=constants.WATERFALL_INTERNAL,
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
        active_waterfall=constants.WATERFALL_INTERNAL,
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
        active_waterfall=constants.WATERFALL_INTERNAL,
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


class MasterSlaveSyncCompletionStageTestWithMasterPaladin(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests MasterSlaveSyncCompletionStage with master-paladin."""
  BOT_ID = 'master-paladin'

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'

    self.PatchObject(buildbucket_lib, 'GetServiceAccount',
                     return_value=True)
    self.PatchObject(auth.AuthorizedHttp, '__init__',
                     return_value=None)
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     'SendBuildbucketRequest',
                     return_value=None)
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     '_GetHost',
                     return_value=buildbucket_lib.BUILDBUCKET_TEST_HOST)
    self.mock_handle_failure = self.PatchObject(
        completion_stages.MasterSlaveSyncCompletionStage, 'HandleFailure')

    self._Prepare()

  def ConstructStage(self):
    sync_stage = sync_stages.ManifestVersionedSyncStage(self._run)

    scheduled_slaves_list = {
        ('build_1', 'buildbucket_id_1', 1),
        ('build_2', 'buildbucket_id_2', 2)
    }
    self._run.attrs.metadata.ExtendKeyListWithList(
        constants.METADATA_SCHEDULED_SLAVES, scheduled_slaves_list)
    return completion_stages.MasterSlaveSyncCompletionStage(
        self._run, sync_stage, success=True)

  def testPerformStageWithFatalFailure(self):
    """Test PerformStage on master-paladin."""
    stage = self.ConstructStage()

    stage._run.attrs.manifest_manager = mock.MagicMock()

    statuses = {
        'build_1': builder_status_lib.BuilderStatus(
            constants.BUILDER_STATUS_INFLIGHT, None),
        'build_2': builder_status_lib.BuilderStatus(
            constants.BUILDER_STATUS_MISSING, None)
    }

    self.PatchObject(completion_stages.MasterSlaveSyncCompletionStage,
                     '_FetchSlaveStatuses', return_value=statuses)
    mock_annotate = self.PatchObject(
        completion_stages.MasterSlaveSyncCompletionStage,
        '_AnnotateFailingBuilders')

    with self.assertRaises(completion_stages.ImportantBuilderFailedException):
      stage.PerformStage()
    mock_annotate.assert_called_once_with(
        set(), {'build_1'}, {'build_2'}, statuses, False)
    self.mock_handle_failure.assert_called_once_with(
        set(), {'build_1'}, {'build_2'}, False)

    mock_annotate.reset_mock()
    self.mock_handle_failure.reset_mock()
    stage._run.attrs.metadata.UpdateWithDict(
        {constants.SELF_DESTRUCTED_BUILD: True})
    with self.assertRaises(completion_stages.ImportantBuilderFailedException):
      stage.PerformStage()
    mock_annotate.assert_called_once_with(
        set(), {'build_1'}, {'build_2'}, statuses, True)
    self.mock_handle_failure.assert_called_once_with(
        set(), {'build_1'}, {'build_2'}, True)

  def testAnnotateBuildStatusFromBuildbucket(self):
    """Test AnnotateBuildStatusFromBuildbucket"""
    stage = self.ConstructStage()

    scheduled_slaves_list = {
        ('build_1', 'buildbucket_id_1', 1),
        ('build_2', 'buildbucket_id_2', 2)
    }
    stage.buildbucket_info_dict = (
        buildbucket_lib.GetScheduledBuildDict(scheduled_slaves_list))

    mock_logging_link = self.PatchObject(
        logging, 'PrintBuildbotLink',
        side_effect=logging.PrintBuildbotLink)
    mock_logging_text = self.PatchObject(
        logging, 'PrintBuildbotStepText',
        side_effect=logging.PrintBuildbotStepText)

    no_stat = set(['not_scheduled_build_1'])
    stage._AnnotateBuildStatusFromBuildbucket(no_stat)
    mock_logging_text.assert_called_once_with(
        '%s wasn\'t scheduled by master.' % 'not_scheduled_build_1')

    build_content = {
        'build': {
            'status': 'COMPLETED',
            'result': 'FAILURE',
            'url': 'dashboard_url',
            "failure_reason": "BUILD_FAILURE",
        }
    }
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     'GetBuildRequest',
                     return_value=build_content)
    no_stat = set(['build_1'])
    stage._AnnotateBuildStatusFromBuildbucket(no_stat)
    mock_logging_link.assert_called_once_with(
        '%s: [status] %s [result] %s [failure_reason] %s' %
        ('build_1', 'COMPLETED', 'FAILURE', 'BUILD_FAILURE'),
        'dashboard_url')

    mock_logging_link.reset_mock()
    build_content = {
        'build': {
            'status': 'COMPLETED',
            'result': 'CANCELED',
            'url': 'dashboard_url',
            "cancelation_reason": "CANCELED_EXPLICITLY",
        }
    }
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     'GetBuildRequest',
                     return_value=build_content)
    no_stat = set(['build_1'])
    stage._AnnotateBuildStatusFromBuildbucket(no_stat)
    mock_logging_link.assert_called_once_with(
        '%s: [status] %s [result] %s [cancelation_reason] %s' %
        ('build_1', 'COMPLETED', 'CANCELED', 'CANCELED_EXPLICITLY'),
        'dashboard_url')


    mock_logging_text.reset_mock()
    self.PatchObject(buildbucket_lib.BuildbucketClient,
                     'GetBuildRequest',
                     side_effect=buildbucket_lib.BuildbucketResponseException)
    no_stat = set(['build_1'])
    stage._AnnotateBuildStatusFromBuildbucket(no_stat)
    mock_logging_text.assert_called_once_with(
        'No status found for build %s buildbucket_id %s' %
        ('build_1', 'buildbucket_id_1'))

  def test_AnnotateNoStatBuildersWithBuildbucketSchedulder(self):
    """Tests _AnnotateNoStatBuilders with master using Buildbucket scheduler."""
    stage = self.ConstructStage()

    annotate_mock = self.PatchObject(
        completion_stages.MasterSlaveSyncCompletionStage,
        '_AnnotateBuildStatusFromBuildbucket')

    no_stat = {'no_stat_1', 'no_stat_2'}
    stage._AnnotateNoStatBuilders(no_stat)
    annotate_mock.assert_called_once_with(no_stat)

  def testAnnotateFailingBuilders(self):
    """Tests that _AnnotateFailingBuilders is free of syntax errors."""
    stage = self.ConstructStage()

    annotate_mock = self.PatchObject(
        completion_stages.MasterSlaveSyncCompletionStage,
        '_AnnotateNoStatBuilders')

    failing = {'failing_build'}
    inflight = {'inflight_build'}
    no_stat = {'no_stat_build'}
    failed_msg = failures_lib.BuildFailureMessage(
        'message', [], True, 'reason', 'bot')
    failed_status = builder_status_lib.BuilderStatus(
        'failed', failed_msg, 'url')
    inflight_status = builder_status_lib.BuilderStatus('inflight', None, 'url')
    statuses = {'failing_build' : failed_status,
                'inflight_build': inflight_status}

    stage._AnnotateFailingBuilders(failing, inflight, set(), statuses, False)
    self.assertEqual(annotate_mock.call_count, 1)

    stage._AnnotateFailingBuilders(failing, inflight, no_stat, statuses, False)
    self.assertEqual(annotate_mock.call_count, 2)

    stage._AnnotateFailingBuilders(failing, inflight, no_stat, statuses, True)
    self.assertEqual(annotate_mock.call_count, 3)


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
    self.PatchObject(relevant_changes.RelevantChanges,
                     '_GetSlaveMappingAndCLActions',
                     return_value=(dict(), []))
    self.PatchObject(clactions, 'GetRelevantChangesForBuilds')
    self.PatchObject(tree_status, 'WaitForTreeStatus',
                     return_value=constants.TREE_OPEN)
    self.PatchObject(relevant_changes.RelevantChanges,
                     'GetPreviouslyPassedSlavesForChanges')

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
                  handle_timeout=False, sane_tot=True,
                  alert=False, stage=None, all_slaves=None, slave_stages=None,
                  build_passed=False):
    """Runs and Verifies PerformStage.

    Args:
      failing: The names of the builders that failed.
      inflight: The names of the buiders that timed out.
      handle_failure: If True, calls HandleValidationFailure.
      handle_timeout: If True, calls HandleValidationTimeout.
      sane_tot: If not true, assumes TOT is not sane.
      alert: If True, sends out an alert email for infra failures.
      stage: If set, use this constructed stage, otherwise create own.
      all_slaves: Optional set of all slave configs.
      slave_stages: Optional list of slave stages.
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
      statuses[x] = builder_status_lib.BuilderStatus(
          constants.BUILDER_STATUS_FAILED, message=None)
    for x in inflight:
      statuses[x] = builder_status_lib.BuilderStatus(
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
    self.PatchObject(stage.sync_stage.pool, 'SubmitPartialPool',
                     return_value=self.other_changes)

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
        self.alert_email_mock.assert_called_once_with(
            mock.ANY, mock.ANY, server=mock.ANY, message=mock.ANY,
            extra_fields=mock.ANY)

      if handle_failure:
        stage.sync_stage.pool.handle_failure_mock.assert_called_once_with(
            mock.ANY, no_stat=set([]), sanity=sane_tot,
            changes=self.other_changes, failed_hwtests=mock.ANY)

      if handle_timeout:
        stage.sync_stage.pool.handle_timeout_mock.assert_called_once_with(
            sanity=mock.ANY, changes=self.other_changes)


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

  def tearDown(self):
    cidb.CIDBConnectionFactory.ClearMock()

  def testNoInflightBuildersWithInfraFail(self):
    """Test case where there are no inflight builders but are infra failures."""
    failing = ['foo']
    inflight = []

    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=['msg'])
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildersWithNoneMessages', return_value=[])
    # An alert is sent, since there are infra failures.
    self.VerifyStage(failing, inflight, alert=True)

  def testMissingCriticalStage(self):
    """Test case where a slave failed to run a critical stage."""
    self.VerifyStage(['foo'], [], slave_stages=[])

  def testFailedCriticalStage(self):
    """Test case where a slave failed a critical stage."""
    fake_stages = [{'name': 'CommitQueueSync', 'build_config': 'foo',
                    'status': constants.BUILDER_STATUS_FAILED}]
    self.VerifyStage(['foo'], [],
                     slave_stages=fake_stages)

  def testMissingCriticalStageOnSanitySlave(self):
    """Test case where a sanity slave failed to run a critical stage."""
    stage = self.ConstructStage()
    fake_stages = [{'name': 'CommitQueueSync', 'build_config': 'foo',
                    'status': constants.BUILDER_STATUS_PASSED}]
    stage._run.config.sanity_check_slaves = ['sanity']
    self.VerifyStage(['sanity', 'foo'], [], slave_stages=fake_stages,
                     stage=stage)

  def testMissingCriticalStageOnTimedOutSanitySlave(self):
    """Test case where a sanity slave failed to run a critical stage."""
    stage = self.ConstructStage()
    fake_stages = [{'name': 'CommitQueueSync', 'build_config': 'foo',
                    'status': constants.BUILDER_STATUS_PASSED}]
    stage._run.config.sanity_check_slaves = ['sanity']
    self.VerifyStage(['foo'], ['sanity'], slave_stages=fake_stages,
                     stage=stage, handle_failure=False, handle_timeout=True)

  def testNoInflightBuildersWithNoneFailureMessages(self):
    """Test case where failed builders reported NoneType messages."""
    failing = ['foo']
    inflight = []

    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=[])
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildersWithNoneMessages', return_value=['foo'])
    # An alert is sent, since NonType messages are considered infra failures.
    self.VerifyStage(failing, inflight, alert=True)

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

  def test_IsFailureFatalWithCLs(self):
    """Test _IsFailureFatal with CLs."""
    stage = self.ConstructStage()
    stage.sync_stage.pool.HasPickedUpCLs.return_value = True

    self.assertFalse(stage._IsFailureFatal(set(), set(), set()))
    self.assertTrue(stage._IsFailureFatal(set(['test3']), set(), set()))

  def test_IsFailureFatalWithoutCLs(self):
    """Test _IsFailureFatal without CLs."""
    stage = self.ConstructStage()
    stage.sync_stage.pool.HasPickedUpCLs.return_value = False

    self.assertFalse(stage._IsFailureFatal(set(), set(), set()))
    self.assertFalse(stage._IsFailureFatal(set(['test3']), set(), set()))

  def testSendInfraAlertIfNeededWithAlerts(self):
    """Test SendInfraAlertIfNeeded which sends alerts."""
    mock_send_alert = self.PatchObject(tree_status, 'SendHealthAlert')
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=['failure_message'])
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildersWithNoneMessages', return_value=[])
    stage = self.ConstructStage()
    failing = {'failing_build'}
    inflight = {'inflight_build'}
    no_stat = {'no_stat_build'}

    stage.SendInfraAlertIfNeeded(failing, inflight, no_stat, False)
    self.assertEqual(mock_send_alert.call_count, 1)
    stage.SendInfraAlertIfNeeded(failing, inflight, no_stat, True)
    self.assertEqual(mock_send_alert.call_count, 2)

  def testSendInfraAlertIfNeededWithoutAlerts(self):
    """Test SendInfraAlertIfNeeded which doesn't send alerts."""
    mock_send_alert = self.PatchObject(tree_status, 'SendHealthAlert')
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetInfraFailMessages', return_value=[])
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildersWithNoneMessages', return_value=[])
    stage = self.ConstructStage()
    inflight = {'inflight_build'}
    no_stat = {'no_stat_build'}

    stage.SendInfraAlertIfNeeded({}, inflight, no_stat, True)
    self.assertEqual(mock_send_alert.call_count, 0)

  def test_GetBuildsPassedSyncStage(self):
    """Test _GetBuildsPassedSyncStage."""
    stage = self.ConstructStage()
    mock_cidb = mock.Mock()
    mock_cidb.GetSlaveStages.return_value = [
        {'build_config': 's_1', 'status': 'pass', 'name': 'CommitQueueSync'},
        {'build_config': 's_2', 'status': 'pass', 'name': 'CommitQueueSync'},
        {'build_config': 's_3', 'status': 'fail', 'name': 'CommitQueueSync'}]
    mock_cidb.GetBuildStages.return_value = [
        {'status': 'pass', 'name': 'CommitQueueSync'}]

    builds = stage._GetBuildsPassedSyncStage(
        'build_id', mock_cidb, ['id_1', 'id_2'])
    self.assertItemsEqual(builds, ['s_1', 's_2', 'master-paladin'])

  def _MockPartialSubmit(self, stage):
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     'SendInfraAlertIfNeeded')
    self.PatchObject(tree_status, 'SendHealthAlert')
    self.PatchObject(relevant_changes.RelevantChanges,
                     'GetRelevantChangesForSlaves',
                     return_value={'master-paladin': {mock.Mock()}})
    self.PatchObject(relevant_changes.RelevantChanges,
                     'GetSubsysResultForSlaves')
    self.PatchObject(completion_stages.CommitQueueCompletionStage,
                     '_GetBuildsPassedSyncStage')
    stage.sync_stage.pool.SubmitPartialPool.return_value = self.changes

  def testCQMasterHandleFailureWithOpenTree(self):
    """Test CQMasterHandleFailure with open tree."""
    stage = self.ConstructStage()
    self._MockPartialSubmit(stage)
    self.PatchObject(tree_status, 'WaitForTreeStatus',
                     return_value=constants.TREE_OPEN)

    stage.CQMasterHandleFailure(set(['test1']), set(), set(), False, [])
    stage.sync_stage.pool.handle_failure_mock.assert_called_once_with(
        mock.ANY, sanity=True, no_stat=set(), changes=self.changes,
        failed_hwtests=None)

  def testCQMasterHandleFailureWithThrottledTree(self):
    """Test CQMasterHandleFailure with throttled tree."""
    stage = self.ConstructStage()
    self._MockPartialSubmit(stage)
    self.PatchObject(tree_status, 'WaitForTreeStatus',
                     return_value=constants.TREE_THROTTLED)

    stage.CQMasterHandleFailure(set(['test1']), set(), set(), False, [])
    stage.sync_stage.pool.handle_failure_mock.assert_called_once_with(
        mock.ANY, sanity=False, no_stat=set(), changes=self.changes,
        failed_hwtests=None)

  def testCQMasterHandleFailureWithClosedTree(self):
    """Test CQMasterHandleFailure with cloased tree."""
    stage = self.ConstructStage()
    self._MockPartialSubmit(stage)
    self.PatchObject(tree_status, 'WaitForTreeStatus',
                     side_effect=timeout_util.TimeoutError())

    stage.CQMasterHandleFailure(set(['test1']), set(), set(), False, [])
    stage.sync_stage.pool.handle_failure_mock.assert_called_once_with(
        mock.ANY, sanity=False, no_stat=set(), changes=self.changes,
        failed_hwtests=None)

  def testCQMasterHandleFailureWithFailedHWtests(self):
    """Test CQMasterHandleFailure with failed HWtests."""
    stage = self.ConstructStage()
    self._MockPartialSubmit(stage)
    master_build_id = stage._run.attrs.metadata.GetValue('build_id')
    db = fake_cidb.FakeCIDBConnection()
    slave_build_id = db.InsertBuild(
        'slave_1', constants.WATERFALL_INTERNAL, 1, 'slave_1', 'bot_hostname',
        master_build_id=master_build_id, buildbucket_id='123')
    cidb.CIDBConnectionFactory.SetupMockCidb(db)
    mock_failed_hwtests = mock.Mock()
    mock_get_hwtests = self.PatchObject(
        hwtest_results.HWTestResultManager,
        'GetFailedHWTestsFromCIDB', return_value=mock_failed_hwtests)
    self.PatchObject(tree_status, 'WaitForTreeStatus',
                     return_value=constants.TREE_OPEN)

    stage.CQMasterHandleFailure(set(['test1']), set(), set(), False, ['123'])
    stage.sync_stage.pool.handle_failure_mock.assert_called_once_with(
        mock.ANY, sanity=True, no_stat=set(), changes=self.changes,
        failed_hwtests=mock_failed_hwtests)
    mock_get_hwtests.assert_called_once_with(db, [slave_build_id])


class PreCQCompletionStageTest(generic_stages_unittest.AbstractStageTestCase):
  """PreCQCompletionStage tests"""
  BOT_ID = 'lumpy-pre-cq'

  def ConstructStage(self):
    sync_stage = mock.Mock()
    return completion_stages.PreCQCompletionStage(
        self._run, sync_stage, success=True)

  def _Prepare(self, bot_id=None, **kwargs):
    super(PreCQCompletionStageTest, self)._Prepare(bot_id, **kwargs)

  def testGetBuildFailureMessage(self):
    """Test GetBuildFailureMessage."""
    db = fake_cidb.FakeCIDBConnection()
    cidb.CIDBConnectionFactory.SetupMockCidb(db)

    build_id = db.InsertBuild('lumpy-pre-cq', constants.WATERFALL_INTERNAL, 1,
                              'lumpy-pre-cq', 'bot_hostname',
                              status=constants.BUILDER_STATUS_INFLIGHT)
    stage_id = db.InsertBuildStage(build_id, 'BuildPackages', status='fail')
    db.InsertFailure(stage_id, 'PackageBuildFailure',
                     'Packages failed in ./build_packages: sys-apps/flashrom',
                     exception_category='build',
                     extra_info={"shortname": "./build_packages",
                                 "failed_packages": ["sys-apps/flashrom"]})
    self._Prepare(build_id=build_id)
    stage = self.ConstructStage()
    message = stage.GetBuildFailureMessage()

    self.assertFalse(message.MatchesExceptionCategory(
        constants.EXCEPTION_CATEGORY_LAB))
    self.assertTrue(message.MatchesExceptionCategory(
        constants.EXCEPTION_CATEGORY_BUILD))


class PublishUprevChangesStageTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests for the PublishUprevChanges stage."""
  BOT_ID = 'master-chromium-pfq'

  def _Prepare(self, bot_id=None, **kwargs):
    super(PublishUprevChangesStageTest, self)._Prepare(bot_id, **kwargs)

  def setUp(self):
    self.PatchObject(completion_stages.PublishUprevChangesStage,
                     '_GetPortageEnvVar')
    self.PatchObject(completion_stages.PublishUprevChangesStage,
                     '_ExtractOverlays', return_value=[['foo'], ['bar']])
    self.PatchObject(prebuilts.BinhostConfWriter, 'Perform')
    self.push_mock = self.PatchObject(commands, 'UprevPush')
    self.PatchObject(generic_stages.BuilderStage, 'GetRepoRepository')
    self.PatchObject(commands, 'UprevPackages')

    self._Prepare()

  def ConstructStage(self):
    sync_stage = sync_stages.ManifestVersionedSyncStage(self._run)
    sync_stage.pool = mock.MagicMock()
    return completion_stages.PublishUprevChangesStage(
        self._run, sync_stage, success=True)

  def testPush(self):
    """Test values for PublishUprevChanges."""
    self._Prepare(extra_config={'push_overlays': constants.PUBLIC_OVERLAYS},
                  extra_cmd_args=['--chrome_rev', constants.CHROME_REV_TOT])
    self._run.options.prebuilts = True
    self.RunStage()
    self.push_mock.assert_called_once_with(self.build_root, ['bar'], False,
                                           staging_branch=None)
    self.assertTrue(self._run.attrs.metadata.GetValue('UprevvedChrome'))
    metadata_dict = self._run.attrs.metadata.GetDict()
    self.assertFalse(metadata_dict.has_key('UprevvedAndroid'))

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

  def testAndroidPush(self):
    """Test values for PublishUprevChanges with Android PFQ."""
    self._Prepare(bot_id=constants.MNC_ANDROID_PFQ_MASTER,
                  extra_config={'push_overlays': constants.PUBLIC_OVERLAYS},
                  extra_cmd_args=['--android_rev',
                                  constants.ANDROID_REV_LATEST])
    self._run.options.prebuilts = True
    self.RunStage()
    self.push_mock.assert_called_once_with(self.build_root, ['bar'], False,
                                           staging_branch=None)
    self.assertTrue(self._run.attrs.metadata.GetValue('UprevvedAndroid'))
    metadata_dict = self._run.attrs.metadata.GetDict()
    self.assertFalse(metadata_dict.has_key('UprevvedChrome'))

  def testPerformStageOnChromePFQ(self):
    """Test PerformStage on ChromePFQ."""
    stage = self.ConstructStage()
    stage.sync_stage.pool.HasPickedUpCLs.return_value = True
    stage.PerformStage()
    self.push_mock.assert_called_once_with(self.build_root, ['bar'], False,
                                           staging_branch=None)

  def testPerformStageOnCQMasterWithPickedUpCLs(self):
    """Test PerformStage on CQ-master with picked up CLs."""
    self._Prepare(bot_id=constants.CQ_MASTER)
    stage = self.ConstructStage()
    stage.sync_stage.pool.HasPickedUpCLs.return_value = True
    stage.PerformStage()
    self.push_mock.assert_called_once_with(self.build_root, ['bar'], False,
                                           staging_branch=None)

  def testPerformStageOnCQMasterWithoutPickedUpCLs(self):
    """Test PerformStage on CQ-master without picked up CLs."""
    self._Prepare(bot_id=constants.CQ_MASTER)
    stage = self.ConstructStage()
    stage.sync_stage.pool.HasPickedUpCLs.return_value = False
    stage.PerformStage()
    self.push_mock.assert_not_called()
