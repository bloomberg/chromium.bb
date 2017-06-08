# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to use goma from buildbot."""

from __future__ import print_function

import collections
import datetime
import glob
import json
import os
import tempfile

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import path_util


_GOMA_LOG_URL_TEMPLATE = (
    'http://chromium-build-stats.appspot.com/compiler_proxy_log/%s/%s')


class Goma(object):
  """Interface to use goma on bots."""

  # Default environment variables to use goma.
  _DEFAULT_ENV_VARS = {
      # Set MAX_COMPILER_DISABLED_TASKS to let goma enter Burst mode, if
      # there are too many local fallback failures. In the Burst mode, goma
      # tries to use CPU cores as many as possible. Note that, by default,
      # goma runs only a few local fallback tasks in parallel at once.
      # The value is the threashold of the number of local fallback failures
      # to enter the mode.
      # Note that 30 is just heuristically chosen by discussion with goma team.
      #
      # Specifically, this is short-term workaround of the case that all
      # compile processes get local fallback. Otherwise, because goma uses only
      # several processes for local fallback by default, it causes significant
      # slow down of the build.
      # Practically, this happens when toolchain is updated in repository,
      # but prebuilt package is not yet ready. (cf. crbug.com/728971)
      'GOMA_MAX_COMPILER_DISABLED_TASKS': '30'
  }

  def __init__(self, goma_dir, goma_client_json, goma_tmp_dir=None):
    """Initializes Goma instance.

    This ensures that |self.goma_log_dir| directory exists (if missing,
    creates it).

    Args:
      goma_dir: Path to the goma directory (outside of chroot).
      goma_client_json: Path to the service account json file to use goma.
        On bots, this must be specified, otherwise raise a ValueError.
        On local, this is optional, and can be set to None.
      goma_tmp_dir: Path to the GOMA_TMP_DIR to be passed to goma programs.
        If given, it is used. If not given, creates a directory under
        /tmp in the chroot, expecting that the directory is removed in the
        next run's clean up phase on bots.

    Raises:
       ValueError if 1) |goma_dir| does not point to a directory, 2)
       on bots, but |goma_client_json| is not given, 3) |goma_client_json|
       is given, but it does not point to a file, or 4) if |goma_tmp_dir| is
       given but it does not point to a directory.
    """
    # Sanity checks of given paths.
    if not os.path.isdir(goma_dir):
      raise ValueError('goma_dir does not point a directory: %s' % (goma_dir,))

    # If this script runs on bot, service account json file needs to be
    # provided, otherwise it cannot access to goma service.
    if cros_build_lib.HostIsCIBuilder() and goma_client_json is None:
      raise ValueError(
          'goma is enabled on bot, but goma_client_json is not provided')

    # If goma_client_json file is provided, it must be an existing file.
    if goma_client_json and not os.path.isfile(goma_client_json):
      raise ValueError(
          'Goma client json file is missing: %s' % (goma_client_json,))

    # If goma_tmp_dir is provided, it must be an existing directory.
    if goma_tmp_dir and not os.path.isdir(goma_tmp_dir):
      raise ValueError(
          'GOMA_TMP_DIR does not point a directory: %s' %(goma_tmp_dir,))

    self.goma_dir = goma_dir
    self.goma_client_json = goma_client_json

    if goma_tmp_dir is None:
      # If |goma_tmp_dir| is not given, create GOMA_TMP_DIR (goma
      # compiler_proxy's working directory), and its log directory.
      # Create unique directory by mkdtemp under chroot's /tmp.
      # Expect this directory is removed in next run's clean up phase.
      goma_tmp_dir = tempfile.mkdtemp(
          prefix='goma_tmp_dir.', dir=path_util.FromChrootPath('/tmp'))
    self.goma_tmp_dir = goma_tmp_dir

    # Create log directory if not exist.
    if not os.path.isdir(self.goma_log_dir):
      os.mkdir(self.goma_log_dir)

  @property
  def goma_log_dir(self):
    """Path to goma's log directory."""
    return os.path.join(self.goma_tmp_dir, 'log_dir')

  def GetChrootExtraEnv(self):
    """Extra env vars set to use goma inside chroot."""
    # Note: GOMA_DIR and GOMA_SERVICE_ACCOUNT_JSON_FILE in chroot is hardcoded.
    # Please see also enter_chroot.sh.
    result = dict(
        Goma._DEFAULT_ENV_VARS,
        GOMA_DIR=os.path.join('/home', os.environ.get('USER'), 'goma'),
        GOMA_TMP_DIR=path_util.ToChrootPath(self.goma_tmp_dir),
        GLOG_log_dir=path_util.ToChrootPath(self.goma_log_dir))
    if self.goma_client_json:
      result['GOMA_SERVICE_ACCOUNT_JSON_FILE'] = (
          '/creds/service_accounts/service-account-goma-client.json')
    return result

  def UploadLogs(self):
    """Uploads INFO files related to goma.

    Returns:
      URL to the compiler_proxy log visualizer. None if unavailable.
    """
    uploader = GomaLogUploader(self.goma_log_dir)
    compiler_proxy_path = uploader.Upload()
    if not compiler_proxy_path:
      return None
    return _GOMA_LOG_URL_TEMPLATE % (
        uploader.dest_path, os.path.basename(compiler_proxy_path) + '.gz')


# Note: Public for testing purpose. In real use, please think about using
# Goma.UploadLogs() instead.
class GomaLogUploader(object):
  """Manages to upload goma log files."""

  # The Google Cloud Storage bucket to store logs related to goma.
  _BUCKET = 'chrome-goma-log'

  def __init__(self, goma_log_dir, today=None, dry_run=False):
    """Initializes the uploader.

    Args:
      goma_log_dir: path to the directory containing goma's INFO log files.
      today: datetime.date instance representing today. This is for testing
        purpose, because datetime.date is unpatchable. In real use case,
        this must be None.
      dry_run: If True, no actual upload. This is for testing purpose.
    """
    self._goma_log_dir = goma_log_dir
    logging.info('Goma log directory is: %s', self._goma_log_dir)

    # Set log upload destination.
    if today is None:
      today = datetime.date.today()
    self.dest_path = '%s/%s' % (
        today.strftime('%Y/%m/%d'), cros_build_lib.GetHostName())
    self._remote_dir = 'gs://%s/%s' % (GomaLogUploader._BUCKET, self.dest_path)
    logging.info('Goma log upload destination: %s', self._remote_dir)

    # Build metadata to be annotated to log files.
    # Use OrderedDict for json output stabilization.
    builder_info = json.dumps(collections.OrderedDict([
        ('builder', os.environ.get('BUILDBOT_BUILDERNAME', '')),
        ('master', os.environ.get('BUILDBOT_MASTERNAME', '')),
        ('slave', os.environ.get('BUILDBOT_SLAVENAME', '')),
        ('clobber', bool(os.environ.get('BUILDBOT_CLOBBER'))),
        ('os', 'chromeos'),
    ]))
    logging.info('BuilderInfo: %s', builder_info)
    self._headers = ['x-goog-meta-builderinfo:' + builder_info]

    self._gs_context = gs.GSContext(dry_run=dry_run)

  def Upload(self):
    """Uploads all necessary log files to Google Storage.

    Returns:
      Path to compiler proxy log file. None if there is not.
    """
    compiler_proxy_subproc_paths = self._UploadInfoFiles(
        'compiler_proxy-subproc')
    # compiler_proxy-subproc.INFO file should be exact one.
    if len(compiler_proxy_subproc_paths) != 1:
      logging.warning('Unexpected compiler_proxy-subproc INFO files: %r',
                      compiler_proxy_subproc_paths)

    compiler_proxy_paths = self._UploadInfoFiles('compiler_proxy')
    # compiler_proxy.INFO file should be exact one.
    if len(compiler_proxy_paths) != 1:
      logging.warning('Unexpected compiler_proxy INFO files: %r',
                      compiler_proxy_paths)

    # TODO(crbug.com/719843): Enable uploading gomacc logs after
    # crbug.com/719843 is resolved.

    return compiler_proxy_paths[0] if compiler_proxy_paths else None

  def _UploadInfoFiles(self, pattern):
    """Uploads INFO files matched with pattern, with gzip'ing.

    Args:
      pattern: matching path pattern.

    Returns:
      A list of uploaded file paths.
    """
    # Find files matched with the pattern in |goma_log_dir|. Sort for
    # stabilization.
    paths = sorted(glob.glob(
        os.path.join(self._goma_log_dir, '%s.*.INFO.*' % pattern)))
    if not paths:
      logging.warning('No glog files matched with: %s', pattern)

    for path in paths:
      logging.info('Uploading %s', path)
      self._gs_context.CopyInto(
          path, self._remote_dir, filename=os.path.basename(path) + '.gz',
          auto_compress=True, headers=self._headers)
    return paths
