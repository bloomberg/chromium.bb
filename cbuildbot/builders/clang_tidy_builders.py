# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for creating clang-tidy builds."""

from __future__ import print_function

from chromite.cbuildbot.builders import generic_builders
from chromite.cbuildbot.stages import artifact_stages
from chromite.cbuildbot.stages import build_stages
from chromite.cbuildbot.stages import chrome_stages


class ClangTidyBuilder(generic_builders.ManifestVersionedBuilder):
  """Builder that creates builds for clang-tidy warnings in Chrome OS."""

  def RunStages(self):
    """Run stages for clang-tidy builder."""
    assert len(self._run.config.boards) == 1
    board = self._run.config.boards[0]

    self._RunStage(build_stages.UprevStage)
    self._RunStage(build_stages.InitSDKStage)
    self._RunStage(build_stages.SetupBoardStage, board)
    self._RunStage(chrome_stages.SyncChromeStage)
    self._RunStage(build_stages.BuildPackagesStage, board)
    self._RunStage(artifact_stages.GenerateTidyWarningsStage, board)
