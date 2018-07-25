# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build stages specific to firmware builds.

Firmware builds use a mix of standard stages, and custom stages
related to how build artifacts are generated and published.
"""

from __future__ import print_function

import os

from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import generic_stages
from chromite.cbuildbot.stages import workspace_stages
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import gs


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

  @property
  def firmware_name(self):
    """Uniqify the name across boards."""
    return '%s_%s' % (self._current_board, constants.FIRMWARE_ARCHIVE_NAME)

  def DummyArchiveUrl(self):
    """Where do the 'dummy' build artifacts go."""
    workspace_version_info = self.GetWorkspaceVersionInfo()

    if self._run.debug:
      build_id, _ = self._run.GetCIDBHandle()
      board_bot = '%s-firmware-tryjob' % self._current_board
      firmware_version = 'R%s-%s-b%s' % (workspace_version_info.chrome_branch,
                                         workspace_version_info.VersionString(),
                                         build_id)
    else:
      board_bot = '%s-firmware' % self._current_board
      firmware_version = 'R%s-%s' % (workspace_version_info.chrome_branch,
                                     workspace_version_info.VersionString())

    return os.path.join(config_lib.GetConfig().params.ARCHIVE_URL,
                        board_bot,
                        firmware_version)

  @property
  def board_download_url(self):
    if self._options.buildbot or self._options.remote_trybot:
      # Translate the gs:// URI to the URL for downloading the same files.
      # TODO(akeshet): The use of a special download url is a workaround for
      # b/27653354. If that is ultimately fixed, revisit this workaround.
      # This download link works for directories.
      return self.upload_url.replace(
          'gs://', gs.PRIVATE_BASE_HTTPS_DOWNLOAD_URL)
    else:
      return self.archive_path

  def CreateBoardFirmwareArchive(self):
    """Create/publish the firmware build artifact for the current board."""
    commands.BuildFirmwareArchive(
        self._build_root, self._current_board,
        self.archive_path, self.firmware_name)
    self.UploadArtifact(self.firmware_name, archive=False)

  def CreateBoardBuildResult(self):
    """Publish a dummy build result for a traditional firmware build."""
    logging.info('Generating firmware artifacts for %s', self._current_board)

    # What file are we uploading?
    firmware_path = os.path.join(self.archive_path, self.firmware_name)

    dummy_archive_url = self.DummyArchiveUrl()

    gs_context = gs.GSContext(acl=self.acl, dry_run=self._run.debug)
    gs_context.CopyInto(firmware_path, dummy_archive_url,
                        filename=constants.FIRMWARE_ARCHIVE_NAME)

    dummy_http_url = gs.GsUrlToHttp(dummy_archive_url,
                                    public=False, directory=True)
    logging.PrintBuildbotLink(constants.FIRMWARE_ARCHIVE_NAME, dummy_http_url)

  def PerformStage(self):
    """Archive and publish the firmware build artifacts."""
    self.CreateBoardFirmwareArchive()
    self.CreateBoardBuildResult()
