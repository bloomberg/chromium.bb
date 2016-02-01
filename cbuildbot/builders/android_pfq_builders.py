# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the Android PFQ builder."""

from __future__ import print_function

from chromite.cbuildbot.builders import simple_builders
from chromite.cbuildbot.stages import android_stages
from chromite.cbuildbot.stages import artifact_stages
from chromite.cbuildbot.stages import build_stages
from chromite.cbuildbot.stages import chrome_stages
from chromite.cbuildbot.stages import test_stages


class AndroidPFQBuilder(simple_builders.SimpleBuilder):
  """Builder that performs Android uprev per overlay."""

  def RunStages(self):
    """Runs through the stages of the Android PFQ slave build."""
    self.RunEarlySyncAndSetupStages()
    self._RunStage(android_stages.SyncAndroidStage)
    self.RunBuildTestStages()
    self.RunBuildStages()


class AndroidPFQMasterBuilder(simple_builders.SimpleBuilder):
  """Builder that performs Android uprev per overlay."""

  def RunStages(self):
    """Runs through the stages of the Android PFQ master build."""
    self._RunStage(build_stages.UprevStage)
    self._RunStage(build_stages.InitSDKStage)
    # The CQ/Chrome PFQ master will not actually run the SyncChrome stage, but
    # we want the logic that gets triggered when SyncChrome stage is skipped.
    self._RunStage(chrome_stages.SyncChromeStage)
    self._RunStage(android_stages.SyncAndroidStage)
    self._RunStage(test_stages.BinhostTestStage)
    self._RunStage(test_stages.BranchUtilTestStage)
    self._RunStage(artifact_stages.MasterUploadPrebuiltsStage)
