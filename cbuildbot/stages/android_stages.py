# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the Android stages."""

from __future__ import print_function

import os

from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import failures_lib
from chromite.lib import gs
from chromite.lib import osutils
from chromite.lib import portage_util
from chromite.lib import results_lib

ANDROIDPIN_MASK_PATH = os.path.join(constants.SOURCE_ROOT,
                                    constants.CHROMIUMOS_OVERLAY_DIR,
                                    'profiles', 'default', 'linux',
                                    'package.mask', 'androidpin')


def _GetAndroidVersionFromMetadata(metadata):
  """Return the Android version from metadata; None if is does not exist.

  Sync stages may have set this metadata to use a specific Android version.
  """
  version_dict = metadata.GetDict().get('version', {})
  return version_dict.get('android')


class UprevAndroidStage(generic_stages.BuilderStage,
                        generic_stages.ArchivingStageMixin):
  """Stage that uprevs Android container if needed."""

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    if not self._android_rev:
      logging.info('Not uprevving Android.')
      return

    android_package = self._run.config.android_package
    android_build_branch = self._run.config.android_import_branch
    android_version = _GetAndroidVersionFromMetadata(self._run.attrs.metadata)
    logging.info('Android package: %s', android_package)
    logging.info('Android branch: %s', android_build_branch)
    logging.info('Android version: %s', android_version or 'LATEST')

    try:
      android_atom_to_build = commands.MarkAndroidAsStable(
          buildroot=self._build_root,
          tracking_branch=self._run.manifest_branch,
          android_package=android_package,
          android_build_branch=android_build_branch,
          boards=self._boards,
          android_version=android_version)
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

    logging.info('android_atom_to_build = %s', android_atom_to_build)

    if (not android_atom_to_build and
        self._run.options.buildbot and
        self._run.config.build_type == constants.ANDROID_PFQ_TYPE):
      logging.info('Android already uprevved. Nothing else to do.')
      raise failures_lib.ExitEarlyException(
          'UprevAndroidStage finished and exited early.')


class AndroidMetadataStage(generic_stages.BuilderStage,
                           generic_stages.ArchivingStageMixin):
  """Stage that records Android container version in metadata.

  This should attempt to generate four types of metadata:
  - a unique Android version if it exists.
  - a unique Android branch if it exists.
  - a per-board Android version for each board.
  - a per-board arc USE flag value.
  """

  def __init__(self, builder_run, **kwargs):
    super(AndroidMetadataStage, self).__init__(builder_run, **kwargs)
    # PerformStage() will fill this out for us.
    self.android_version = None
    self.android_branch = None

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    # Initially get version from metadata in case the initial sync
    # stage set it.
    self.android_version = _GetAndroidVersionFromMetadata(
        self._run.attrs.metadata)

    # Need to always iterate through and generate the board-specific
    # Android version metadata.  Each board must be handled separately
    # since there might be differing builds in the same release group.
    versions = set([])
    branches = set([])
    for builder_run in self._run.GetUngroupedBuilderRuns():
      for board in builder_run.config.boards:
        try:
          # Determine the version for each board and record metadata.
          version = self._run.DetermineAndroidVersion(boards=[board])
          builder_run.attrs.metadata.UpdateBoardDictWithDict(
              board, {'android-container-version': version})
          versions.add(version)
          logging.info('Board %s has Android version %s', board, version)
        except cbuildbot_run.NoAndroidVersionError as ex:
          logging.info('Board %s does not contain Android (%s)', board, ex)
        try:
        # Determine the branch for each board and record metadata.
          branch = self._run.DetermineAndroidBranch(board)
          builder_run.attrs.metadata.UpdateBoardDictWithDict(
              board, {'android-container-branch': branch})
          branches.add(branch)
          logging.info('Board %s has Android branch %s', board, branch)
        except cbuildbot_run.NoAndroidBranchError as ex:
          logging.info('Board %s does not contain Android (%s)', board, ex)
        arc_use = self._run.HasUseFlag(board, 'arc')
        logging.info('Board %s %s arc USE flag set.', board,
                     'has' if arc_use else 'does not have')
        builder_run.attrs.metadata.UpdateBoardDictWithDict(
            board, {'arc-use-set': arc_use})
    # If there wasn't a version or branch specified in the manifest but there is
    # a unique one across all the boards, treat it as the version for the
    # entire step.
    if self.android_version is None and len(versions) == 1:
      self.android_version = versions.pop()
    if self.android_branch is None and len(branches) == 1:
      self.android_branch = branches.pop()

    if self.android_version:
      logging.PrintBuildbotStepText('tag %s' % self.android_version)
    if self.android_branch:
      logging.PrintBuildbotStepText('branch %s' % self.android_branch)

  def _WriteAndroidVersionToMetadata(self):
    """Write Android version to metadata and upload partial json file."""
    # Even if the stage failed, a None value for android_version still
    # means something.  In other words, this stage tried to run.
    self._run.attrs.metadata.UpdateKeyDictWithDict(
        'version',
        {'android': self.android_version,
         'android-branch': self.android_branch})
    self.UploadMetadata(filename=constants.PARTIAL_METADATA_JSON)

  def Finish(self):
    """Provide android_version to the rest of the run."""
    self._WriteAndroidVersionToMetadata()
    super(AndroidMetadataStage, self).Finish()


class DownloadAndroidDebugSymbolsStage(generic_stages.BoardSpecificBuilderStage,
                                       generic_stages.ArchivingStageMixin):
  """Stage that downloads Android debug symbols.

  Downloaded archive will be picked up by DebugSymbolsStage.
  """

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def PerformStage(self):
    if not config_lib.IsCanaryType(self._run.config.build_type):
      logging.info('This stage runs only in release builders.')
      return

    # Get the Android versions set by AndroidMetadataStage.
    version_dict = self._run.attrs.metadata.GetDict().get('version', {})
    android_build_branch = version_dict.get('android-branch')
    android_version = version_dict.get('android')

    # On boards not supporting Android, versions will be None.
    if not (android_build_branch and android_version):
      logging.info('Android is not enabled on this board. Skipping.')
      return

    logging.info(
        'Downloading symbols of Android %s (%s)...',
        android_version, android_build_branch)

    board_use_flags = portage_util.GetBoardUseFlags(self._current_board)
    arch = None
    for arch_use_flag, arch in constants.ARC_USE_FLAG_TO_ARCH.items():
      if arch_use_flag in board_use_flags:
        break
    if not arch:
      raise AssertionError(
          'Could not determine the arch of %s.' % self._current_board)

    symbols_file_url = constants.ANDROID_SYMBOLS_URL_TEMPLATE % {
        'branch': android_build_branch,
        'arch': arch,
        'version': android_version}
    symbols_file = os.path.join(self.archive_path,
                                constants.ANDROID_SYMBOLS_FILE)
    gs_context = gs.GSContext()
    gs_context.Copy(symbols_file_url, symbols_file)
