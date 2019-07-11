# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for creating fuzzer builds."""

from __future__ import print_function

from chromite.cbuildbot.builders import simple_builders
from chromite.cbuildbot.stages import artifact_stages
from chromite.cbuildbot.stages import build_stages


class FuzzerBuilder(simple_builders.SimpleBuilder):
  """Builder that creates builds for fuzzing Chrome OS."""

  def RunStages(self):
    """Run stages for fuzzer builder."""
    assert len(self._run.config.boards) == 1
    board = self._run.config.boards[0]

    self._RunStage(build_stages.UprevStage)
    self._RunStage(build_stages.InitSDKStage)
    self._RunStage(build_stages.UpdateSDKStage)
    self._RunStage(build_stages.RegenPortageCacheStage)
    self._RunStage(build_stages.SetupBoardStage, board)
    self._RunStage(build_stages.BuildPackagesStage, board,
                   record_packages_under_test=False)
    self._RunStage(artifact_stages.GenerateSysrootStage, board)
    self._RunStage(artifact_stages.ArchiveStage, board, builder_run=self._run)
