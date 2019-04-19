# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""'Builders' that just run actions in the chroot without building boards."""

from __future__ import print_function

from chromite.cbuildbot.builders import generic_builders
from chromite.cbuildbot.stages import afdo_stages
from chromite.cbuildbot.stages import build_stages


class ReleaseProfileMergeBuilder(generic_builders.ManifestVersionedBuilder):
  """Builder that generates merged profiles for Chrome."""

  def RunStages(self):
    """Run stages for the release AFDO profile merge builder."""
    assert len(self._run.config.boards) == 0

    # All we need is a reasonably functional SDK, so we can run llvm-profdata
    # and a few gsutil commands in it.
    self._RunStage(build_stages.UprevStage)
    self._RunStage(build_stages.InitSDKStage)
    self._RunStage(build_stages.UpdateSDKStage)
    self._RunStage(afdo_stages.AFDOReleaseProfileMergerStage)
