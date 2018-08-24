# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing firmware builders."""

from __future__ import print_function

from chromite.cbuildbot.builders import generic_builders
from chromite.cbuildbot.stages import build_stages
from chromite.cbuildbot.stages import firmware_stages
from chromite.cbuildbot.stages import workspace_stages


class FirmwareBranchBuilder(generic_builders.ManifestVersionedBuilder):
  """Builder that builds firmware branches.

  This builder checks out a second copy of ChromeOS into the workspace
  on the firmware branch, and performs a firmware build there for 1 or
  more boards, and publishes all of the results to both it's own build
  artifacts, as well as to a second location specific to each board
  that looks like a the results of a traditional single board firmware
  build.
  """

  def RunStages(self):
    """Run the stages."""
    workspace_dir = self._run.options.workspace

    self._RunStage(workspace_stages.WorkspaceCleanStage,
                   build_root=workspace_dir)

    self._RunStage(workspace_stages.WorkspaceSyncStage,
                   build_root=workspace_dir)

    self._RunStage(workspace_stages.WorkspaceUprevAndPublishStage,
                   build_root=workspace_dir)

    self._RunStage(workspace_stages.WorkspacePublishBuildspecStage,
                   build_root=workspace_dir)

    self._RunStage(build_stages.InitSDKStage,
                   build_root=workspace_dir)

    for board in self._run.config.boards:
      self._RunStage(workspace_stages.WorkspaceSetupBoardStage,
                     build_root=workspace_dir,
                     board=board)

      self._RunStage(workspace_stages.WorkspaceBuildPackagesStage,
                     build_root=workspace_dir,
                     board=board)

      self._RunStage(firmware_stages.FirmwareArchiveStage,
                     build_root=workspace_dir,
                     board=board)
