# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build stages specific to firmware builds.

Firmware builds use a mix of standard stages, and custom stages
related to how build artifacts are generated and published.
"""

from __future__ import print_function

import json
import os

from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import workspace_stages
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import osutils


class UnsafeBuildForPushImage(Exception):
  """Raised if push_image is run against a non-signable build."""


class FirmwareArchiveStage(workspace_stages.WorkspaceStageBase,
                           generic_stages.BoardSpecificBuilderStage,
                           generic_stages.ArchivingStageMixin):
  """Generates and publishes firmware specific build artifacts.

  This stage publishes <board>_firmware_from_source.tar.bz2 to this
  builds standard build artifacts, and also generates a 'fake' build
  result (called a Dummy result) that looks like it came from a
  traditional style firmware builder for a single board on the
  firmware branch.

  The secondary result only contains firmware_from_source.tar.bz2 and
  a dummy version of metadata.json.
  """

  def __init__(self, builder_run, build_root, **kwargs):
    """Initializer.

    Args:
      builder_run: BuilderRun object.
      build_root: Fully qualified path to use as a string.
    """
    super(FirmwareArchiveStage, self).__init__(
        builder_run, build_root=build_root, **kwargs)

    self.dummy_firmware_config = None
    self.workspace_version_info = None
    self.firmware_version = None

  @property
  def metadata_name(self):
    """Uniqify the name across boards."""
    return '%s_%s' % (self._current_board, constants.METADATA_JSON)

  @property
  def firmware_name(self):
    """Uniqify the name across boards."""
    return '%s_%s' % (self._current_board, constants.FIRMWARE_ARCHIVE_NAME)

  @property
  def dummy_archive_url(self):
    """Uniqify the name across boards."""
    return os.path.join(
        config_lib.GetConfig().params.ARCHIVE_URL,
        self.dummy_firmware_config,
        self.firmware_version)

  def CreateBoardFirmwareArchive(self):
    """Create/publish the firmware build artifact for the current board."""
    logging.info('Create %s', self.firmware_name)

    commands.BuildFirmwareArchive(
        self._build_root, self._current_board,
        self.archive_path, self.firmware_name)
    self.UploadArtifact(self.firmware_name, archive=False)

  def CreateBoardMetadataJson(self):
    """Create/publish the firmware build artifact for the current board."""
    logging.info('Create %s', self.metadata_name)

    board_metadata_path = os.path.join(self.archive_path, self.metadata_name)

    # Use the metadata for the main build, with selected fields modified.
    board_metadata = self._run.attrs.metadata.GetDict()
    board_metadata['boards'] = [self._current_board]
    board_metadata['branch'] = self._run.config.workspace_branch
    board_metadata['version_full'] = self.firmware_version
    board_metadata['version_milestone'] = \
        self.workspace_version_info.chrome_branch
    board_metadata['version_platform'] = \
        self.workspace_version_info.VersionString()

    logging.info('Writing firmware metadata to %s.', board_metadata_path)
    osutils.WriteFile(board_metadata_path, json.dumps(board_metadata),
                      atomic=True, makedirs=True)

    self.UploadArtifact(self.metadata_name, archive=False)

  def CreateBoardBuildResult(self):
    """Publish a dummy build result for a traditional firmware build."""
    logging.info('Publish "Dummy" firmware build for GE. %s',
                 self._current_board)

    # What file are we uploading?
    firmware_path = os.path.join(self.archive_path, self.firmware_name)
    metadata_path = os.path.join(self.archive_path, self.metadata_name)

    gs_context = gs.GSContext(acl=self.acl, dry_run=self._run.options.debug)
    gs_context.CopyInto(firmware_path, self.dummy_archive_url,
                        filename=constants.FIRMWARE_ARCHIVE_NAME)
    gs_context.CopyInto(metadata_path, self.dummy_archive_url,
                        filename=constants.METADATA_JSON)

    dummy_http_url = gs.GsUrlToHttp(self.dummy_archive_url,
                                    public=False, directory=True)

    label = '%s Firmware' % self._current_board
    logging.PrintBuildbotLink(label, dummy_http_url)

  def PushBoardImage(self):
    """Helper method to run push_image against the dummy boards artifacts."""
    # This helper script is only available on internal manifests currently.
    if not self._run.config['internal']:
      raise UnsafeBuildForPushImage("Can't use push_image on external builds.")

    logging.info('Use pushimage to publish signing artifacts for: %s',
                 self._current_board)

    # Push build artifacts to gs://chromeos-releases for signing and release.
    # This runs TOT pushimage against the build artifacts for the branch.
    commands.PushImages(
        board=self._current_board,
        archive_url=self.dummy_archive_url,
        dryrun=self._run.options.debug or not self._run.config['push_image'],
        profile=self._run.options.profile or self._run.config['profile'],
        sign_types=self._run.config['sign_types'] or [],
        buildroot=self._build_root)

  def PerformStage(self):
    """Archive and publish the firmware build artifacts."""
    # Populate shared values.
    self.workspace_version_info = self.GetWorkspaceVersionInfo()

    if self._run.options.debug:
      build_id, _ = self._run.GetCIDBHandle()
      self.dummy_firmware_config = '%s-firmware-tryjob' % self._current_board
      self.firmware_version = 'R%s-%s-b%s' % (
          self.workspace_version_info.chrome_branch,
          self.workspace_version_info.VersionString(),
          build_id)
    else:
      self.dummy_firmware_config = '%s-firmware' % self._current_board
      self.firmware_version = 'R%s-%s' % (
          self.workspace_version_info.chrome_branch,
          self.workspace_version_info.VersionString())

    logging.info('Firmware version: %s', self.firmware_version)
    logging.info('Archive firmware build as: %s', self.dummy_firmware_config)

    self.CreateBoardFirmwareArchive()
    self.CreateBoardMetadataJson()
    self.CreateBoardBuildResult()
    self.PushBoardImage()
