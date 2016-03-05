# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the Android stages."""

from __future__ import print_function

import os
import sys

from chromite.cbuildbot import cbuildbot_run
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


class UprevAndroidStage(generic_stages.BuilderStage,
                        generic_stages.ArchivingStageMixin):
  """Stage that uprevs Android container if needed."""
  # TODO: Consider merging this with SyncChromeStage.

  def __init__(self, builder_run, **kwargs):
    super(UprevAndroidStage, self).__init__(builder_run, **kwargs)
    # PerformStage() will fill this out for us.
    self.android_version = None

  def HandleSkip(self):
    """Set run.attrs.android_version to Android version in buildroot now."""
    try:
      self._run.attrs.android_version = self._run.DetermineAndroidVersion(
          boards=self._boards)
      logging.debug('Existing Android version is %s.',
                    self._run.attrs.android_version)
      self._WriteAndroidVersionToMetadata()
    except cbuildbot_run.NoAndroidVersionError as ex:
      logging.warn('Unable to determine Android version (%s)', ex)
    super(UprevAndroidStage, self).HandleSkip()

  def _GetAndroidVersionFromMetadata(self):
    """Return the Android version from metadata; None if is does not exist."""
    version_dict = self._run.attrs.metadata.GetDict().get('version', {})
    return version_dict.get('android')

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    android_atom_to_build = None
    if self._android_rev:
      self.android_version = self._GetAndroidVersionFromMetadata()
      if self.android_version:
        logging.info('Using Android version from the metadata dictionary: %s',
                     self.android_version)

      try:
        android_atom_to_build = commands.MarkAndroidAsStable(
            self._build_root,
            self._run.manifest_branch,
            self._boards,
            android_version=self.android_version)
      except commands.AndroidIsPinnedUprevError as e:
        # If uprev failed due to a pin, record that failure (so that the
        # build ultimately fails) but try again without the pin, to allow the
        # slave to test the newer version anyway).
        android_atom_to_build = e.new_android_atom
        if android_atom_to_build:
          results_lib.Results.Record(self.name, e)
          logging.PrintBuildbotStepFailure()
          logging.error('Android is pinned. Attempting to continue build for '
                        'Android atom %s anyway but build will ultimately '
                        'fail.',
                        android_atom_to_build)
          logging.info('Deleting pin file at %s and proceeding.',
                       ANDROIDPIN_MASK_PATH)
          osutils.SafeUnlink(ANDROIDPIN_MASK_PATH)
        else:
          raise

    kwargs = {}
    if self.android_version is None:
      try:
        self.android_version = self._run.DetermineAndroidVersion(
            boards=self._boards)
      except cbuildbot_run.NoAndroidVersionError as ex:
        if android_atom_to_build is not None:
          logging.error('Unable to determine Android version, uprevved %s',
                        android_atom_to_build)
          raise
        else:
          logging.info('Build does not contain Android (%s)', ex)


    if self.android_version is not None:
      kwargs['tag'] = self.android_version
      logging.PrintBuildbotStepText('tag %s' % kwargs['tag'])

    if (self._android_rev and
        not android_atom_to_build and
        self._run.options.buildbot and
        self._run.config.build_type == constants.ANDROID_PFQ_TYPE):
      logging.info('Android already uprevved. Nothing else to do.')
      sys.exit(0)

  def _WriteAndroidVersionToMetadata(self):
    """Write Android version to metadata and upload partial json file."""
    self._run.attrs.metadata.UpdateKeyDictWithDict(
        'version',
        {'android': self._run.attrs.android_version})
    self.UploadMetadata(filename=constants.PARTIAL_METADATA_JSON)

  def _Finish(self):
    """Provide android_version to the rest of the run."""
    # Even if the stage failed, a None value for android_version still
    # means something.  In other words, this stage tried to run.
    self._run.attrs.android_version = self.android_version
    self._WriteAndroidVersionToMetadata()
    super(UprevAndroidStage, self)._Finish()
