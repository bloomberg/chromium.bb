# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the Android PFQ builder."""

from __future__ import print_function

from chromite.cbuildbot import results_lib
from chromite.cbuildbot.builders import simple_builders
from chromite.cbuildbot.stages import android_stages
from chromite.cbuildbot.stages import artifact_stages
from chromite.cbuildbot.stages import build_stages
from chromite.cbuildbot.stages import chrome_stages
from chromite.cbuildbot.stages import sync_stages
from chromite.cbuildbot.stages import test_stages
from chromite.lib import cros_logging as logging


class AndroidPFQBuilder(simple_builders.SimpleBuilder):
  """Builder that performs Android uprev per overlay."""

  def RunStages(self):
    """Runs through the stages of the Android PFQ slave build."""
    self.RunEarlySyncAndSetupStages()
    self._RunStage(android_stages.SyncAndroidStage)
    self.RunBuildTestStages()
    self.RunBuildStages()


class AndroidPFQMasterBuilder(simple_builders.DistributedBuilder):
  """Builder that performs Android uprev per overlay."""

  def GetSyncInstance(self):
    """Sync using distributed or normal logic as necessary.

    Returns:
      The instance of the sync stage to run.
    """
    if self._run.options.buildbot:
      # Use distributed logic
      sync_stage = super(AndroidPFQMasterBuilder, self).GetSyncInstance()
    else:
      sync_stage = self._GetStageInstance(sync_stages.SyncStage)
    return sync_stage

  def RunStages(self):
    """Runs through the stages of the Android PFQ master build."""
    was_build_successful = False
    build_finished = False
    try:
      self._RunStage(build_stages.UprevStage)
      self._RunStage(build_stages.InitSDKStage)
      # The CQ/Chrome PFQ master will not actually run the SyncChrome stage, but
      # we want the logic that gets triggered when SyncChrome stage is skipped.
      self._RunStage(chrome_stages.SyncChromeStage)
      self._RunStage(android_stages.SyncAndroidStage)
      self._RunStage(test_stages.BinhostTestStage)
      self._RunStage(test_stages.BranchUtilTestStage)
      self._RunStage(artifact_stages.MasterUploadPrebuiltsStage)
      was_build_successful = results_lib.Results.BuildSucceededSoFar()
      build_finished = True

    except SystemExit as ex:
      # If a stage calls sys.exit(0), it's exiting with success, so that means
      # we should mark ourselves as successful.
      logging.info('Detected sys.exit(%s)', ex.code)
      if ex.code == 0:
        was_build_successful = True
      raise
    finally:
      if self._run.options.buildbot:
        self.Publish(was_build_successful, build_finished)
