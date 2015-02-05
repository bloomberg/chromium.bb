# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing various one-off builders."""

from __future__ import print_function

from chromite.cbuildbot.builders import simple_builders
from chromite.cbuildbot.stages import build_stages
from chromite.cbuildbot.stages import report_stages


class RefreshPackagesBuilder(simple_builders.SimpleBuilder):
  """Run the refresh packages status update."""

  def RunStages(self):
    """Runs through build process."""
    self._RunStage(build_stages.InitSDKStage)
    self.RunSetupBoard()
    self._RunStage(report_stages.RefreshPackageStatusStage)
