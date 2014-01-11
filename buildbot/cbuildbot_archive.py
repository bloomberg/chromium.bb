# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module with utilities for archiving functionality."""

import os

from chromite.buildbot import cbuildbot_config
from chromite.buildbot import constants

from chromite.lib import gs


def GetBaseUploadURI(config, archive_base=None, bot_id=None,
                     remote_trybot=False):
  """Get the base URL where artifacts from this builder are uploaded.

  Each build run stores its artifacts in a subdirectory of the base URI.
  We also have LATEST files under the base URI which help point to the
  latest build available for a given builder.

  Args:
    config: The build config to examine.
    archive_base: Optional. The root URL under which objects from all
      builders are uploaded. If not specified, we use the default archive
      bucket.
    bot_id: The bot ID to archive files under.
    remote_trybot: Whether this is a remote trybot run. This is used to
      make sure that uploads from remote trybot runs do not conflict with
      uploads from production builders.

  Returns:
    Google Storage URI (i.e. 'gs://...') under which all archived files
      should be uploaded.  In other words, a path like a directory, even
      through GS has no real directories.
  """
  if not bot_id:
    bot_id = config.GetBotId(remote_trybot=remote_trybot)

  if archive_base:
    return '%s/%s' % (archive_base, bot_id)
  elif remote_trybot or config.gs_path == cbuildbot_config.GS_PATH_DEFAULT:
    return '%s/%s' % (constants.DEFAULT_ARCHIVE_BUCKET, bot_id)
  else:
    return config.gs_path


class Archive(object):
  """Class to represent the archive for one builder run.

  An Archive object is a read-only object with attributes and methods useful
  for archive purposes.  Most of the attributes are supported as properties
  because they depend on the ChromeOS version and if they are calculated too
  soon (i.e. before the sync stage) they will raise an exception.

  Attributes:
    archive_path: The full local path where output from this builder is stored.
    download_url: The URL where we can download artifacts.
    upload_url: The Google Storage location where we should upload artifacts.
    version: The ChromeOS version for this archive.
  """

  _BUILDBOT_ARCHIVE = 'buildbot_archive'
  _TRYBOT_ARCHIVE = 'trybot_archive'

  def __init__(self, bot_id, version_getter, options, config):
    """Initialize.

    Args:
      bot_id: The bot id associated with this archive.
      version_getter: Functor that should return the ChromeOS version for
        this run when called, if the version is known.  Typically, this
        is BuilderRun.GetVersion.
      options: The command options object for this run.
      config: The build config for this run.
    """
    self._options = options
    self._config = config
    self._version_getter = version_getter
    self._version = None

    self.bot_id = bot_id

  @property
  def version(self):
    if self._version is None:
      self._version = self._version_getter()

    return self._version

  @property
  def archive_path(self):
    return os.path.join(self.GetLocalArchiveRoot(), self.bot_id, self.version)

  @property
  def upload_url(self):
    base_upload_url = GetBaseUploadURI(
        self._config,
        archive_base=self._options.archive_base,
        bot_id=self.bot_id,
        remote_trybot=self._options.remote_trybot)
    return '%s/%s' % (base_upload_url, self.version)

  @property
  def download_url(self):
    if self._options.buildbot or self._options.remote_trybot:
      # Translate the gs:// URI to the URL for downloading the same files.
      return self.upload_url.replace('gs://', gs.PRIVATE_BASE_HTTPS_URL)
    else:
      return self.archive_path

  def GetLocalArchiveRoot(self, trybot=None):
    """Return the location on disk where archive images are kept."""
    buildroot = os.path.abspath(self._options.buildroot)

    if trybot is None:
      trybot = not self._options.buildbot or self._options.debug

    archive_base = self._TRYBOT_ARCHIVE if trybot else self._BUILDBOT_ARCHIVE
    return os.path.join(buildroot, archive_base)
