# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for report stages."""

from __future__ import print_function

import mock
import os

from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import cbuildbot_unittest
from chromite.cbuildbot import commands
from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import manifest_version
from chromite.cbuildbot import metadata_lib
from chromite.cbuildbot import results_lib
from chromite.cbuildbot import triage_lib
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.cbuildbot.stages import report_stages
from chromite.lib import alerts
from chromite.lib import cidb
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import fake_cidb
from chromite.lib import gs_unittest
from chromite.lib import osutils
from chromite.lib import patch_unittest
from chromite.lib import retry_stats
from chromite.lib import toolchain


# pylint: disable=protected-access
# pylint: disable=too-many-ancestors


class BuildReexecutionStageTest(generic_stages_unittest.AbstractStageTestCase):
  """Tests that BuildReexecutionFinishedStage behaves as expected."""
  def setUp(self):
    self.fake_db = fake_cidb.FakeCIDBConnection()
    cidb.CIDBConnectionFactory.SetupMockCidb(self.fake_db)
    build_id = self.fake_db.InsertBuild(
        'builder name', 'waterfall', 1, 'build config', 'bot hostname')

    self._Prepare(build_id=build_id)

    release_tag = '4815.0.0-rc1'
    self._run.attrs.release_tag = '4815.0.0-rc1'
    fake_versioninfo = manifest_version.VersionInfo(release_tag, '39')
    self.gs_mock = self.StartPatcher(gs_unittest.GSContextMock())
    self.gs_mock.SetDefaultCmdResult()
    self.PatchObject(cbuildbot_run._BuilderRunBase, 'GetVersionInfo',
                     return_value=fake_versioninfo)
    self.PatchObject(toolchain, 'GetToolchainsForBoard')

  def tearDown(self):
    cidb.CIDBConnectionFactory.SetupMockCidb()

  def testPerformStage(self):
    """Test that a normal runs completes without error."""
    self.RunStage()

  def testMasterSlaveVersionMismatch(self):
    """Test that master/slave version mismatch causes failure."""
    master_release_tag = '9999.0.0-rc1'
    master_build_id = self.fake_db.InsertBuild(
        'master', constants.WATERFALL_INTERNAL, 2, 'master config',
        'master hostname')
    master_metadata = metadata_lib.CBuildbotMetadata()
    master_metadata.UpdateKeyDictWithDict(
        'version', {'full' : 'R39-9999.0.0-rc1',
                    'milestone': '39',
                    'platform': master_release_tag})
    self._run.attrs.metadata.UpdateWithDict(
        {'master_build_id': master_build_id})
    self.fake_db.UpdateMetadata(master_build_id, master_metadata)

    stage = self.ConstructStage()
    with self.assertRaises(failures_lib.StepFailure):
      stage.Run()

  def ConstructStage(self):
    return report_stages.BuildReexecutionFinishedStage(self._run)


class ConfigDumpStageTest(generic_stages_unittest.AbstractStageTestCase):
  """Tests that ConfigDumpStage runs without syntax error."""

  def ConstructStage(self):
    return report_stages.ConfigDumpStage(self._run)

  def testPerformStage(self):
    self._Prepare()
    self.RunStage()


class SlaveFailureSummaryStageTest(
    generic_stages_unittest.AbstractStageTestCase):
  """Tests that SlaveFailureSummaryStage behaves as expected."""

  def setUp(self):
    self.db = mock.MagicMock()
    cidb.CIDBConnectionFactory.SetupMockCidb(self.db)
    self._Prepare(build_id=1)

  def _Prepare(self, **kwargs):
    """Prepare stage with config['master']=True."""
    super(SlaveFailureSummaryStageTest, self)._Prepare(**kwargs)
    self._run.config['master'] = True

  def ConstructStage(self):
    return report_stages.SlaveFailureSummaryStage(self._run)

  def testPerformStage(self):
    """Tests that stage runs without syntax errors."""
    fake_failure = {
        'build_id': 10,
        'build_stage_id': 11,
        'waterfall': constants.WATERFALL_EXTERNAL,
        'builder_name': 'builder_name',
        'build_number': 12,
        'build_config': 'build-config',
        'stage_name': 'FailingStage',
        'stage_status': constants.BUILDER_STATUS_FAILED,
        'build_status': constants.BUILDER_STATUS_FAILED,
        }
    self.PatchObject(self.db, 'GetSlaveFailures', return_value=[fake_failure])
    self.PatchObject(logging, 'PrintBuildbotLink')
    self.RunStage()
    self.assertEqual(logging.PrintBuildbotLink.call_count, 1)


class BuildStartStageTest(generic_stages_unittest.AbstractStageTestCase):
  """Tests that BuildStartStage behaves as expected."""

  def setUp(self):
    self.db = fake_cidb.FakeCIDBConnection()
    cidb.CIDBConnectionFactory.SetupMockCidb(self.db)
    retry_stats.SetupStats()
    os.environ['BUILDBOT_MASTERNAME'] = constants.WATERFALL_EXTERNAL

    master_build_id = self.db.InsertBuild(
        'master_build', constants.WATERFALL_EXTERNAL, 1,
        'master_build_config', 'bot_hostname')

    self._Prepare(build_id=None, master_build_id=master_build_id)

  def testUnknownWaterfall(self):
    """Test that an assertion is thrown if master name is not valid."""
    os.environ['BUILDBOT_MASTERNAME'] = 'gibberish'
    self.assertRaises(failures_lib.StepFailure, self.RunStage)

  def testPerformStage(self):
    """Test that a normal run of the stage does a database insert."""
    self.RunStage()

    build_id = self._run.attrs.metadata.GetValue('build_id')
    self.assertGreater(build_id, 0)
    self.assertEqual(self._run.attrs.metadata.GetValue('db_type'),
                     cidb.CONNECTION_TYPE_MOCK)

  def testHandleSkipWithInstanceChange(self):
    """Test that HandleSkip disables cidb and dies when necessary."""
    # This test verifies that switching to a 'mock' database type once
    # metadata already has an id in 'previous_db_type' will fail.
    self._run.attrs.metadata.UpdateWithDict({'build_id': 31337,
                                             'db_type': 'previous_db_type'})
    stage = self.ConstructStage()
    self.assertRaises(AssertionError, stage.HandleSkip)
    self.assertEqual(cidb.CIDBConnectionFactory.GetCIDBConnectionType(),
                     cidb.CONNECTION_TYPE_INV)
    # The above test has the side effect of invalidating CIDBConnectionFactory.
    # Undo that side effect so other unit tests can run.
    cidb.CIDBConnectionFactory.SetupMockCidb()

  def testHandleSkipWithNoDbType(self):
    """Test that HandleSkip passes when db_type is missing."""
    self._run.attrs.metadata.UpdateWithDict({'build_id': 31337})
    stage = self.ConstructStage()
    stage.HandleSkip()

  def testHandleSkipWithDbType(self):
    """Test that HandleSkip passes when db_type is specified."""
    self._run.attrs.metadata.UpdateWithDict(
        {'build_id': 31337,
         'db_type': cidb.CONNECTION_TYPE_MOCK})
    stage = self.ConstructStage()
    stage.HandleSkip()

  def ConstructStage(self):
    return report_stages.BuildStartStage(self._run)


class AbstractReportStageTestCase(
    generic_stages_unittest.AbstractStageTestCase,
    cbuildbot_unittest.SimpleBuilderTestCase):
  """Base class for testing the Report stage."""

  def setUp(self):
    for cmd in ((osutils, 'WriteFile'),
                (commands, 'UploadArchivedFile'),
                (alerts, 'SendEmail')):
      self.StartPatcher(mock.patch.object(*cmd, autospec=True))
    retry_stats.SetupStats()

    # Set up a general purpose cidb mock. Tests with more specific
    # mock requirements can replace this with a separate call to
    # SetupMockCidb
    cidb.CIDBConnectionFactory.SetupMockCidb(mock.MagicMock())

    self._Prepare()

  def _SetupUpdateStreakCounter(self, counter_value=-1):
    self.PatchObject(report_stages.ReportStage, '_UpdateStreakCounter',
                     autospec=True, return_value=counter_value)

  def ConstructStage(self):
    return report_stages.ReportStage(self._run, None)


class ReportStageTest(AbstractReportStageTestCase):
  """Test the Report stage."""

  RELEASE_TAG = ''

  def testCheckResults(self):
    """Basic sanity check for results stage functionality"""
    self._SetupUpdateStreakCounter()
    self.PatchObject(report_stages.ReportStage, '_UploadArchiveIndex',
                     return_value={'any': 'dict'})
    self.RunStage()
    filenames = (
        'LATEST-%s' % self.TARGET_MANIFEST_BRANCH,
        'LATEST-%s' % self.VERSION,
    )
    calls = [mock.call(mock.ANY, mock.ANY, 'metadata.json', False,
                       update_list=True, acl=mock.ANY)]
    calls += [mock.call(mock.ANY, mock.ANY, filename, False,
                        acl=mock.ANY) for filename in filenames]
    self.assertEquals(calls, commands.UploadArchivedFile.call_args_list)

  def testDoNotUpdateLATESTMarkersWhenBuildFailed(self):
    """Check that we do not update the latest markers on failed build."""
    self._SetupUpdateStreakCounter()
    self.PatchObject(report_stages.ReportStage, '_UploadArchiveIndex',
                     return_value={'any': 'dict'})
    self.PatchObject(results_lib.Results, 'BuildSucceededSoFar',
                     return_value=False)
    stage = self.ConstructStage()
    stage.Run()
    calls = [mock.call(mock.ANY, mock.ANY, 'metadata.json', False,
                       update_list=True, acl=mock.ANY)]
    self.assertEquals(calls, commands.UploadArchivedFile.call_args_list)

  def testAlertEmail(self):
    """Send out alerts when streak counter reaches the threshold."""
    self.PatchObject(cbuildbot_run._BuilderRunBase,
                     'InEmailReportingEnvironment', return_value=True)
    self.PatchObject(cros_build_lib, 'HostIsCIBuilder', return_value=True)
    self._Prepare(extra_config={'health_threshold': 3,
                                'health_alert_recipients': ['foo@bar.org']})
    self._SetupUpdateStreakCounter(counter_value=-3)
    self.RunStage()
    # The mocking logic gets confused with SendEmail.
    # pylint: disable=no-member
    self.assertGreater(alerts.SendEmail.call_count, 0,
                       'CQ health alerts emails were not sent.')

  def testAlertEmailOnFailingStreak(self):
    """Continue sending out alerts when streak counter exceeds the threshold."""
    self.PatchObject(cbuildbot_run._BuilderRunBase,
                     'InEmailReportingEnvironment', return_value=True)
    self.PatchObject(cros_build_lib, 'HostIsCIBuilder', return_value=True)
    self._Prepare(extra_config={'health_threshold': 3,
                                'health_alert_recipients': ['foo@bar.org']})
    self._SetupUpdateStreakCounter(counter_value=-5)
    self.RunStage()
    # The mocking logic gets confused with SendEmail.
    # pylint: disable=no-member
    self.assertGreater(alerts.SendEmail.call_count, 0,
                       'CQ health alerts emails were not sent.')

  def testWriteBasicMetadata(self):
    """Test that WriteBasicMetadata writes expected keys correctly."""
    report_stages.WriteBasicMetadata(self._run)
    metadata_dict = self._run.attrs.metadata.GetDict()
    self.assertEqual(metadata_dict['build-number'],
                     generic_stages_unittest.DEFAULT_BUILD_NUMBER)
    self.assertTrue(metadata_dict.has_key('builder-name'))
    self.assertTrue(metadata_dict.has_key('bot-hostname'))

  def testGetChildConfigsMetadataList(self):
    """Test that GetChildConfigListMetadata generates child config metadata."""
    child_configs = [{'name': 'config1', 'boards': ['board1']},
                     {'name': 'config2', 'boards': ['board2']}]
    config_status_map = {'config1': True,
                         'config2': False}
    expected = [{'name': 'config1', 'boards': ['board1'],
                 'status': constants.FINAL_STATUS_PASSED},
                {'name': 'config2', 'boards': ['board2'],
                 'status': constants.FINAL_STATUS_FAILED}]
    child_config_list = report_stages.GetChildConfigListMetadata(
        child_configs, config_status_map)
    self.assertEqual(expected, child_config_list)


class ReportStageNoSyncTest(AbstractReportStageTestCase):
  """Test the Report stage if SyncStage didn't complete.

  If SyncStage doesn't complete, we don't know the release tag, and can't
  archive results.
  """
  RELEASE_TAG = None

  def testCommitQueueResults(self):
    """Check that we can run with a RELEASE_TAG of None."""
    self._SetupUpdateStreakCounter()
    self.RunStage()


class DetectIrrelevantChangesStageTest(
    generic_stages_unittest.AbstractStageTestCase,
    patch_unittest.MockPatchBase):
  """Test the DetectIrrelevantChangesStage."""

  def setUp(self):
    self.changes = self.GetPatches(how_many=2)

    self._Prepare()

  def testGetSubsystemsWithoutEmptyEntry(self):
    """Tests the logic of GetSubsystemTobeTested() under normal case."""
    relevant_changes = self.changes
    self.PatchObject(triage_lib, 'GetTestSubsystemForChange',
                     side_effect=[['light'], ['light', 'power']])

    expected = {'light', 'power'}
    stage = self.ConstructStage()
    results = stage.GetSubsystemToTest(relevant_changes)
    self.assertEqual(results, expected)

  def testGetSubsystemsWithEmptyEntry(self):
    """Tests whether return empty set when have empty entry in subsystems."""
    relevant_changes = self.changes
    self.PatchObject(triage_lib, 'GetTestSubsystemForChange',
                     side_effect=[['light'], []])

    expected = set()
    stage = self.ConstructStage()
    results = stage.GetSubsystemToTest(relevant_changes)
    self.assertEqual(results, expected)

  def ConstructStage(self):
    return report_stages.DetectIrrelevantChangesStage(self._run,
                                                      self._current_board,
                                                      self.changes)
