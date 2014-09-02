#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for report stages."""

import mox
import os
import sys

sys.path.insert(0, os.path.abspath('%s/../../..' % os.path.dirname(__file__)))
from chromite.cbuildbot import commands
from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import validation_pool
from chromite.cbuildbot.cbuildbot_unittest import BuilderRunMock
from chromite.cbuildbot.stages import sync_stages
from chromite.cbuildbot.stages import sync_stages_unittest
from chromite.cbuildbot.stages import report_stages
from chromite.cbuildbot.stages import generic_stages_unittest
from chromite.lib import cidb
from chromite.lib import cros_test_lib
from chromite.lib import alerts
from chromite.lib import osutils


# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock


# pylint: disable=R0901,W0212
class BuildStartStageTest(generic_stages_unittest.AbstractStageTest):
  """Tests that BuildStartStage behaves as expected."""

  def setUp(self):
    self.mock_cidb = mox.MockObject(cidb.CIDBConnection)
    cidb.CIDBConnectionFactory.SetupMockCidb(self.mock_cidb)
    os.environ['BUILDBOT_MASTERNAME'] = 'chromiumos'
    self._Prepare(build_id = None)

  def tearDown(self):
    mox.Verify(self.mock_cidb)

  def testUnknownWaterfall(self):
    """Test that an assertion is thrown is master name is not valid."""
    os.environ['BUILDBOT_MASTERNAME'] = 'gibberish'
    self.assertRaises(failures_lib.StepFailure, self.RunStage)

  def testPerformStage(self):
    """Test that a normal run of the stage does a database insert."""
    self.mock_cidb.InsertBuild(bot_hostname=mox.IgnoreArg(),
                               build_config='x86-generic-paladin',
                               build_number=1234321,
                               builder_name=mox.IgnoreArg(),
                               master_build_id=None,
                               waterfall='chromiumos').AndReturn(31337)
    mox.Replay(self.mock_cidb)
    self.RunStage()
    self.assertEqual(self._run.attrs.metadata.GetValue('build_id'), 31337)
    self.assertEqual(self._run.attrs.metadata.GetValue('db_type'),
                     cidb.CIDBConnectionFactory._CONNECTION_TYPE_MOCK)

  def testHandleSkipWithInstanceChange(self):
    """Test that HandleSkip disables cidb and dies when necessary."""
    # This test verifies that switching to a 'mock' database type once
    # metadata already has an id in 'previous_db_type' will fail.
    self._run.attrs.metadata.UpdateWithDict({'build_id': 31337,
                                             'db_type': 'previous_db_type'})
    stage = self.ConstructStage()
    self.assertRaises(AssertionError, stage.HandleSkip)
    self.assertEqual(cidb.CIDBConnectionFactory.GetCIDBConnectionType(),
                     cidb.CIDBConnectionFactory._CONNECTION_TYPE_INV)
    # The above test has the side effect of invalidating CIDBConnectionFactory.
    # Undo that side effect so other unit tests can run.
    cidb.CIDBConnectionFactory._ClearCIDBSetup()
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
         'db_type': cidb.CIDBConnectionFactory._CONNECTION_TYPE_MOCK})
    stage = self.ConstructStage()
    stage.HandleSkip()

  def ConstructStage(self):
    return report_stages.BuildStartStage(self._run)


# pylint: disable=R0901,W0212
class ReportStageTest(generic_stages_unittest.AbstractStageTest):
  """Test the Report stage."""

  RELEASE_TAG = ''

  def setUp(self):
    for cmd in ((osutils, 'WriteFile'),
                (commands, 'UploadArchivedFile'),
                (alerts, 'SendEmail')):
      self.StartPatcher(mock.patch.object(*cmd, autospec=True))

    self.StartPatcher(BuilderRunMock())
    self.cq = sync_stages_unittest.CLStatusMock()
    self.StartPatcher(self.cq)
    self.sync_stage = None

    # Set up a general purpose cidb mock. Tests with more specific
    # mock requirements can replace this with a separate call to
    # SetupMockCidb
    mock_cidb = mox.MockObject(cidb.CIDBConnection)
    cidb.CIDBConnectionFactory.SetupMockCidb(mock_cidb)

    self._Prepare()

  def _SetupUpdateStreakCounter(self, counter_value=-1):
    self.PatchObject(report_stages.ReportStage, '_UpdateStreakCounter',
                     autospec=True, return_value=counter_value)

  def _SetupCommitQueueSyncPool(self):
    self.sync_stage = sync_stages.CommitQueueSyncStage(self._run)
    pool = validation_pool.ValidationPool(constants.BOTH_OVERLAYS,
        self.build_root, build_number=3, builder_name=self._bot_id,
        is_master=True, dryrun=True)
    pool.changes = [sync_stages_unittest.MockPatch()]
    self.sync_stage.pool = pool

  def ConstructStage(self):
    return report_stages.ReportStage(self._run, self.sync_stage, None)

  def testCheckResults(self):
    """Basic sanity check for results stage functionality"""
    self._SetupUpdateStreakCounter()
    self.PatchObject(report_stages.ReportStage, '_UploadArchiveIndex',
                     return_value={'any': 'dict'})
    self.RunStage()
    filenames = (
        'LATEST-%s' % self.TARGET_MANIFEST_BRANCH,
        'LATEST-%s' % BuilderRunMock.VERSION,
    )
    calls = [mock.call(mock.ANY, mock.ANY, 'metadata.json', False,
                       update_list=True, acl=mock.ANY)]
    calls += [mock.call(mock.ANY, mock.ANY, filename, False,
                        acl=mock.ANY) for filename in filenames]
    # pylint: disable=E1101
    self.assertEquals(calls, commands.UploadArchivedFile.call_args_list)

  def testCommitQueueResults(self):
    """Check that commit queue patches get serialized"""
    self._SetupUpdateStreakCounter()
    self._SetupCommitQueueSyncPool()
    self.RunStage()

  def testAlertEmail(self):
    """Send out alerts when streak counter reaches the threshold."""
    self._Prepare(extra_config={'health_threshold': 3,
                                'health_alert_recipients': ['foo@bar.org']})
    self._SetupUpdateStreakCounter(counter_value=-3)
    self._SetupCommitQueueSyncPool()
    self.RunStage()
    # pylint: disable=E1101
    self.assertGreater(alerts.SendEmail.call_count, 0,
                       'CQ health alerts emails were not sent.')

  def testAlertEmailOnFailingStreak(self):
    """Continue sending out alerts when streak counter exceeds the threshold."""
    self._Prepare(extra_config={'health_threshold': 3,
                                'health_alert_recipients': ['foo@bar.org']})
    self._SetupUpdateStreakCounter(counter_value=-5)
    self._SetupCommitQueueSyncPool()
    self.RunStage()
    # pylint: disable=E1101
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


if __name__ == '__main__':
  cros_test_lib.main()
