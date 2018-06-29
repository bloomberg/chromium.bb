# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build stages related to a secondary workspace directory.

A workspace is a compelete ChromeOS checkout and may contain it's own chroot,
.cache directory, etc. Conceptually, cbuildbot_launch creates a workspace for
the intitial ChromeOS build, but these stages are for creating a secondary
build.

This might be useful if a build needs to work with more than one branch at a
time, or make changes to ChromeOS code without changing the code it is currently
running.

A secondary workspace may not be inside an existing ChromeOS repo checkout.
Also, the initial sync will usually take about 40 minutes, so performance should
be considered carefully.
"""

from __future__ import print_function

from chromite.cbuildbot.stages import generic_stages
from chromite.lib import cros_logging as logging


class WorkspaceStageBase(generic_stages.BuilderStage):
  """Base class for Workspace stages."""
  def __init__(self, builder_run, workspace_dir, **kwargs):
    """Initializer.

    Args:
      builder_run: BuilderRun object.
      workspace_dir: Fully qualified path to use as a string.
    """
    super(WorkspaceStageBase, self).__init__(builder_run, **kwargs)
    self.workspace_dir = workspace_dir


class WorkspaceCleanStage(WorkspaceStageBase):
  """Clean a working directory checkout."""

  def PerformStage(self):
    """Clean stuff!."""
    logging.info('Cleaning: %s', self.workspace_branch)


class WorkspaceSyncStage(WorkspaceStageBase):
  """Clean a working directory checkout."""

  def __init__(self, builder_run, workspace_dir, workspace_branch, **kwargs):
    """Initializer.

    Args:
      builder_run: BuilderRun object.
      workspace_dir: Fully qualified path to use as a string.
      workspace_branch: branch to sync into the workspace.
    """
    super(WorkspaceSyncStage, self).__init__(builder_run, **kwargs)
    self.workspace_branch = workspace_branch

  def PerformStage(self):
    """Sync stuff!."""
    logging.info('Syncing %s branch into %s',
                 self.workspace_branch, self.workspace_dir)
