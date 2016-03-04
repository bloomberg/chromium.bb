# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the Android PFQ builder."""

from __future__ import print_function

from chromite.cbuildbot import results_lib
from chromite.cbuildbot.builders import simple_builders
from chromite.cbuildbot.stages import android_stages
from chromite.cbuildbot.stages import build_stages
from chromite.cbuildbot.stages import chrome_stages
from chromite.cbuildbot.stages import completion_stages
from chromite.cbuildbot.stages import sync_stages
from chromite.cbuildbot.stages import test_stages
from chromite.lib import cros_logging as logging


class AndroidPFQBuilder(simple_builders.SimpleBuilder):
  """Builder that performs Android uprev per overlay."""

  def __init__(self, *args, **kwargs):
    """Initializes a Android PFQ builder."""
    super(AndroidPFQBuilder, self).__init__(*args, **kwargs)
    self.sync_stage = None
    self._completion_stage = None

  def IsDistributed(self):
    """Determines if this builder is being run as a slave.

    Returns:
      True if the build is distributed (ie running as a slave).
    """
    return self._run.options.buildbot and self._run.config['manifest_version']

  def GetSyncInstance(self):
    """Sync using distributed or normal logic as necessary.

    Returns:
      The instance of the sync stage to run.
    """
    if self.IsDistributed():
      self.sync_stage = self._GetStageInstance(
          sync_stages.MasterSlaveLKGMSyncStage)
    else:
      self.sync_stage = self._GetStageInstance(sync_stages.SyncStage)

    return self.sync_stage

  def GetCompletionInstance(self):
    """Returns the completion_stage_class instance that was used for this build.

    Returns:
      None if the completion_stage instance was not yet created (this
      occurs during Publish).
    """
    return self._completion_stage

  def Publish(self, was_build_successful):
    """Completes build by publishing any required information.

    Args:
      was_build_successful: Whether the build succeeded.
    """
    self._completion_stage = self._GetStageInstance(
        completion_stages.MasterSlaveSyncCompletionStage,
        self.sync_stage, was_build_successful)
    self._completion_stage.Run()

  def RunStages(self):
    """Runs through the stages of the Android PFQ slave build."""
    was_build_successful = False
    try:
      self.RunEarlySyncAndSetupStages()
      self._RunStage(android_stages.UprevAndroidStage)
      self.RunBuildTestStages()
      self.RunBuildStages()
      was_build_successful = results_lib.Results.BuildSucceededSoFar()
    except SystemExit as ex:
      # If a stage calls sys.exit(0), it's exiting with success, so that means
      # we should mark ourselves as successful.
      logging.info('Detected sys.exit(%s)', ex.code)
      if ex.code == 0:
        was_build_successful = True
      raise
    finally:
      if self.IsDistributed():
        self.Publish(was_build_successful)


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
      # The PFQ master will not actually run the SyncChrome/UprevAndroid stages,
      # but we want the logic that gets triggered when the stages are skipped.
      self._RunStage(chrome_stages.SyncChromeStage)
      self._RunStage(android_stages.UprevAndroidStage)
      self._RunStage(test_stages.BinhostTestStage)
      self._RunStage(test_stages.BranchUtilTestStage)
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
