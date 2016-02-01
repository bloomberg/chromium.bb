# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the Android stages."""

from __future__ import print_function

import os
import sys

from chromite.cbuildbot import commands
from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import results_lib
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import cros_logging as logging
from chromite.lib import osutils

ANDROIDPIN_MASK_PATH = os.path.join(constants.SOURCE_ROOT,
                                    constants.CHROMIUMOS_OVERLAY_DIR,
                                    'profiles', 'default', 'linux',
                                    'package.mask', 'androidpin')


class SyncAndroidStage(generic_stages.BuilderStage,
                       generic_stages.ArchivingStageMixin):
  """Stage that syncs Android sources if needed."""
  # TODO: Consider merging this with SyncChromeStage.

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    try:
      android_atom_to_build = commands.MarkAndroidAsStable(
          self._build_root,
          self._run.manifest_branch,
          self._boards)
    except commands.AndroidIsPinnedUprevError as e:
      # If uprev failed due to a pin, record that failure (so that the
      # build ultimately fails) but try again without the pin, to allow the
      # slave to test the newer version anyway).
      android_atom_to_build = e.new_android_atom
      if android_atom_to_build:
        results_lib.Results.Record(self.name, e)
        logging.PrintBuildbotStepFailure()
        logging.error('Android is pinned. Attempting to continue build for '
                      'Android atom %s anyway but build will ultimately fail.',
                      android_atom_to_build)
        logging.info('Deleting pin file at %s and proceeding.',
                     ANDROIDPIN_MASK_PATH)
        osutils.SafeUnlink(ANDROIDPIN_MASK_PATH)
      else:
        raise

    if (not android_atom_to_build and
        self._run.options.buildbot and
        self._run.config.build_type == constants.ANDROID_PFQ_TYPE):
      logging.info('Android already uprevved. Nothing else to do.')
      sys.exit(0)
