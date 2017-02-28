# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing builders intended for testing cbuildbot behaviors."""

from __future__ import print_function

from chromite.lib import cros_logging as logging

from chromite.cbuildbot import manifest_version
from chromite.cbuildbot.builders import generic_builders
from chromite.cbuildbot.stages import build_stages
from chromite.cbuildbot.stages import android_stages
from chromite.cbuildbot.stages import chrome_stages
from chromite.cbuildbot.stages import completion_stages
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import sync_stages
from chromite.cbuildbot.stages import test_stages
from chromite.lib import results_lib


class SuccessStage(generic_stages.BuilderStage):
  """Build stage declares success!"""
  def Run(self):
    logging.info('!!!SuccessStage, FTW!!!')


class ManifestVersionedSyncBuilder(generic_builders.Builder):
  """Builder that performs sync, then exits."""

  def GetVersionInfo(self):
    """Returns the CrOS version info from the chromiumos-overlay."""
    return manifest_version.VersionInfo.from_repo(self._run.buildroot)

  def GetSyncInstance(self):
    """Returns an instance of a SyncStage that should be run."""
    return self._GetStageInstance(sync_stages.ManifestVersionedSyncStage)

  def RunStages(self):
    """Run something after sync/reexec."""
    self._RunStage(SuccessStage)


class UnittestStressBuilder(generic_builders.Builder):
  """Builder that runs unittests repeatedly to reproduce flake failures."""

  TEST_CYCLES = 20

  def GetVersionInfo(self):
    """Returns the CrOS version info from the chromiumos-overlay."""
    return manifest_version.VersionInfo.from_repo(self._run.buildroot)

  def GetSyncInstance(self):
    """Returns an instance of a SyncStage that should be run."""
    return self._GetStageInstance(sync_stages.ManifestVersionedSyncStage)

  def RunStages(self):
    """Run something after sync/reexec."""
    assert len(self._run.config.boards) == 1
    board = self._run.config.boards[0]

    self._RunStage(build_stages.UprevStage)
    self._RunStage(build_stages.InitSDKStage)
    self._RunStage(build_stages.RegenPortageCacheStage)
    self._RunStage(build_stages.SetupBoardStage, board)
    self._RunStage(chrome_stages.SyncChromeStage)
    self._RunStage(chrome_stages.PatchChromeStage)
    self._RunStage(android_stages.UprevAndroidStage)
    self._RunStage(android_stages.AndroidMetadataStage)
    self._RunStage(build_stages.BuildPackagesStage, board)

    for i in xrange(self.TEST_CYCLES):
      self._RunStage(test_stages.UnitTestStage, board, suffix=' - %d' % i)


class SignerTestsBuilder(generic_builders.Builder):
  """Builder that runs the cros-signing tests, and nothing else."""

  def __init__(self, *args, **kwargs):
    """Initializes a buildbot builder."""
    super(SignerTestsBuilder, self).__init__(*args, **kwargs)
    self.sync_stage = None
    self.completion_instance = None

  def GetSyncInstance(self):
    """Returns an instance of a SyncStage that should be run."""
    self.sync_stage = self._GetStageInstance(sync_stages.PreCQSyncStage,
                                             self.patch_pool.gerrit_patches)
    self.patch_pool.gerrit_patches = []

    return self.sync_stage

  def GetCompletionInstance(self):
    """Return the completion instance.

    Should not be called until after testing is finished, safe to call
    repeatedly.
    """
    if not self.completion_instance:
      build_id, db = self._run.GetCIDBHandle()
      was_build_successful = results_lib.Results.BuildSucceededSoFar(
          db, build_id)

      self.completion_instance = self._GetStageInstance(
          completion_stages.PreCQCompletionStage,
          self.sync_stage,
          was_build_successful)

    return self.completion_instance

  def RunStages(self):
    """Run something after sync/reexec."""
    self._RunStage(test_stages.CrosSigningTestStage)


class ChromiteTestsBuilder(generic_builders.PreCqBuilder):
  """Builder that runs chromite unit tests, including network."""

  def RunTestStages(self):
    """Run something after sync/reexec."""
    self._RunStage(build_stages.InitSDKStage)
    self._RunStage(test_stages.ChromiteTestStage)
