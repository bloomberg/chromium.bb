# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing firmware builders."""

from __future__ import print_function

from chromite.cbuildbot.builders import generic_builders
from chromite.cbuildbot.stages import workspace_stages


class FirmwareBranchBuilder(generic_builders.ManifestVersionedBuilder):
  """Builder that builds firmware branches."""

  def RunStages(self):
    """Prepare the working directory, and use it for the firmware branch.."""
    workspace_dir = self._run.options.workspace
    firmware_branch = 'master'

    self._RunStage(workspace_stages.WorkspaceCleanStage,
                   workspace_dir)

    self._RunStage(workspace_stages.WorkspaceSyncStage,
                   workspace_dir, firmware_branch)
