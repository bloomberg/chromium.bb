#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for report stages."""

import os
import sys

sys.path.insert(0, os.path.abspath('%s/../../..' % os.path.dirname(__file__)))
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import constants
from chromite.buildbot import validation_pool
from chromite.buildbot.stages import sync_stages
from chromite.buildbot.stages import sync_stages_unittest
from chromite.buildbot.stages import report_stages
from chromite.buildbot.stages import generic_stages_unittest
from chromite.lib import cros_test_lib
from chromite.lib import alerts
from chromite.lib import osutils

from chromite.buildbot.stages.generic_stages_unittest import BuilderRunMock


# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock


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
                                'health_alert_recipients': ['fake_recipient']})
    self._SetupUpdateStreakCounter(counter_value=-3)
    self._SetupCommitQueueSyncPool()
    self.RunStage()
    # pylint: disable=E1101
    self.assertGreater(alerts.SendEmail.call_count, 0,
                       'CQ health alerts emails were not sent.')

  def testAlertEmailOnFailingStreak(self):
    """Continue sending out alerts when streak counter exceeds the threshold."""
    self._Prepare(extra_config={'health_threshold': 3,
                                'health_alert_recipients': ['fake_recipient']})
    self._SetupUpdateStreakCounter(counter_value=-5)
    self._SetupCommitQueueSyncPool()
    self.RunStage()
    # pylint: disable=E1101
    self.assertGreater(alerts.SendEmail.call_count, 0,
                       'CQ health alerts emails were not sent.')


if __name__ == '__main__':
  cros_test_lib.main()
