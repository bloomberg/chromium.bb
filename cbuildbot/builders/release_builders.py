# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing release engineering related builders."""

from __future__ import print_function

from chromite.cbuildbot.builders import simple_builders
from chromite.cbuildbot.stages import branch_stages


class CreateBranchBuilder(simple_builders.SimpleBuilder):
  """Create release branches in the manifest."""

  def RunStages(self):
    """Runs through build process."""
    self._RunStage(branch_stages.BranchUtilStage)
