# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to archive goma logs."""

from __future__ import print_function

import datetime
import getpass
import glob
import json
import os
import shlex
import shutil

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils


class SpecifiedFileMissingError(Exception):
  """Error occurred when running LogsArchiver."""


class LogsArchiver(object):
  """Manages archiving goma log files.

The LogsArchiver was migrated from GomaLogUploader in cbuildbot/goma_util.py.
Unlike the GomaLogUploader, it does not interact with GoogleStorage at all.
Instead it copies Goma files to a client-specified archive directory.
  """

  def __init__(self, log_dir, dest_dir, stats_file=None, counterz_file=None):
    """Initializes the archiver.

    Args:
      log_dir: path to the directory containing goma's INFO log files.
      dest_dir: path to the target directory to which logs are written.
      stats_file: name of stats file in the log_dir.
      counterz_file: name of the counterz file in the log dir.
    """
    self._log_dir = log_dir
    self._stats_file = stats_file
    self._counterz_file = counterz_file
    self._dest_dir = dest_dir
    # Ensure destination dir exists.
    osutils.SafeMakedirs(self._dest_dir)

  def Archive(self):
    """Archives all goma log files, stats file, and counterz file to dest_dir.

    Returns:
      A list of files that were copied to dest_dir.
    """
    log_files = []
    # Find log file names containing compiler_proxy-subproc.INFO.
    # _ArchiveInfoFiles returns a list of tuples of (info_file_path,
    # archived_file_name). We expect only 1 to be found, and add the filename
    # for that tuple to log_files.
    compiler_proxy_subproc_paths = self._ArchiveInfoFiles(
        'compiler_proxy-subproc')
    if len(compiler_proxy_subproc_paths) != 1:
      logging.warning('Unexpected compiler_proxy-subproc INFO files: %r',
                      compiler_proxy_subproc_paths)
    else:
      log_files.append(compiler_proxy_subproc_paths[0][1])

    # Find log file names containing compiler_proxy.INFO.
    # _ArchiveInfoFiles returns a list of tuples of (info_file_path,
    # archived_file_name). We expect only 1 to be found, and then need
    # to use the first tuple value of the list of 1 for the full path, and
    # the filename of the tupe is added to log_files.
    compiler_proxy_path = None
    compiler_proxy_paths = self._ArchiveInfoFiles('compiler_proxy')
    if len(compiler_proxy_paths) != 1:
      logging.warning('Unexpected compiler_proxy INFO files: %r',
                      compiler_proxy_paths)
    else:
      compiler_proxy_path = compiler_proxy_paths[0][0]
      log_files.append(compiler_proxy_paths[0][1])

    gomacc_info_file = self._ArchiveGomaccInfoFiles()
    log_files.append(gomacc_info_file)

    archived_ninja_log_filename = self._ArchiveNinjaLog(compiler_proxy_path)
    if archived_ninja_log_filename:
      log_files.append(archived_ninja_log_filename)

    # Copy stats file and counterz file if they are specified.
    if self._counterz_file:
      counterz_path = os.path.join(self._log_dir, self._counterz_file)
      if not os.path.isfile(counterz_path):
        raise SpecifiedFileMissingError(
            'Goma counterz file missing: ' + counterz_path)
      shutil.copyfile(counterz_path,
                      os.path.join(self._dest_dir, self._counterz_file))
      log_files.append(self._counterz_file)
    if self._stats_file:
      stats_path = os.path.join(self._log_dir, self._stats_file)
      if not os.path.isfile(stats_path):
        raise SpecifiedFileMissingError(
            'Goma stats file missing: ' + stats_path)
      shutil.copyfile(stats_path,
                      os.path.join(self._dest_dir, self._stats_file))
      log_files.append(self._stats_file)
    return log_files

  def _ArchiveInfoFiles(self, pattern):
    """Archives INFO files matched with pattern, with gzip'ing.

    Args:
      pattern: matching path pattern.

    Returns:
      A list of tuples of (info_file_path, archived_file_name).
    """
    # Find files matched with the pattern in |log_dir|. Sort for
    # stabilization.
    paths = sorted(glob.glob(
        os.path.join(self._log_dir, '%s.*.INFO.*' % pattern)))
    if not paths:
      logging.warning('No glog files matched with: %s', pattern)

    result = []
    for path in paths:
      logging.info('Compressing %s', path)
      archived_filename = os.path.basename(path) + '.gz'
      dest_filepath = os.path.join(self._dest_dir, archived_filename)
      cros_build_lib.CompressFile(path, dest_filepath)
      result.append((path, archived_filename))
    return result

  def _ArchiveGomaccInfoFiles(self):
    """Archives gomacc INFO files, with gzip'ing.

    Returns:
      Archived file path. If failed, None.
    """

    # Since the number of gomacc logs can be large, we'd like to compress them.
    # Otherwise, archive will take long (> 10 mins).
    # Each gomacc logs file size must be small (around 4KB).

    # Find files matched with the pattern in |log_dir|.
    # The paths were themselves used as the inputs for the create
    # tarball, but there can be too many of them. As long as we have
    # files we'll just tar up the entire directory.
    gomacc_paths = glob.glob(os.path.join(self._log_dir,
                                          'gomacc.*.INFO.*'))
    if not gomacc_paths:
      # gomacc logs won't be made every time.
      # Only when goma compiler_proxy has
      # crashed. So it's usual gomacc logs are not found.
      logging.info('No gomacc logs found')
      return None

    tgz_name = os.path.basename(min(gomacc_paths)) + '.tar.gz'
    tgz_path = os.path.join(self._dest_dir, tgz_name)
    cros_build_lib.CreateTarball(target=tgz_path,
                                 cwd=self._log_dir,
                                 compression=cros_build_lib.COMP_GZIP)
    return tgz_name

  def _ArchiveNinjaLog(self, compiler_proxy_path):
    """Archives .ninja_log file and its related metadata.

    This archives the .ninja_log file generated by ninja to build Chrome.
    Also, it appends some related metadata at the end of the file following
    '# end of ninja log' marker.

    Args:
      compiler_proxy_path: Path to the compiler proxy, which will be contained
        in the metadata.

    Returns:
      The name of the archived file.
    """
    ninja_log_path = os.path.join(self._log_dir, 'ninja_log')
    if not os.path.exists(ninja_log_path):
      logging.warning('ninja_log is not found: %s', ninja_log_path)
      return None
    ninja_log_content = osutils.ReadFile(ninja_log_path)

    try:
      st = os.stat(ninja_log_path)
      ninja_log_mtime = datetime.datetime.fromtimestamp(st.st_mtime)
    except OSError:
      logging.exception('Failed to get timestamp: %s', ninja_log_path)
      return None

    ninja_log_info = self._BuildNinjaInfo(compiler_proxy_path)

    # Append metadata at the end of the log content.
    ninja_log_content += '# end of ninja log\n' + json.dumps(ninja_log_info)

    # Aligned with goma_utils in chromium bot.
    pid = os.getpid()

    archive_ninja_log_path = os.path.join(
        self._log_dir,
        'ninja_log.%s.%s.%s.%d' % (
            getpass.getuser(), cros_build_lib.GetHostName(),
            ninja_log_mtime.strftime('%Y%m%d-%H%M%S'), pid))
    osutils.WriteFile(archive_ninja_log_path, ninja_log_content)
    archived_filename = os.path.basename(archive_ninja_log_path) + '.gz'

    archived_path = os.path.join(self._dest_dir, archived_filename)
    cros_build_lib.CompressFile(archive_ninja_log_path, archived_path)

    return archived_path

  def _BuildNinjaInfo(self, compiler_proxy_path):
    """Reads metadata for the ninja run.

    Each metadata should be written into a dedicated file in the log directory.
    Read the info, and build the dict containing metadata.

    Args:
      compiler_proxy_path: Path to the compiler_proxy log file.

    Returns:
      A dict of the metadata.
    """

    info = {'platform': 'chromeos'}

    command_path = os.path.join(self._log_dir, 'ninja_command')
    if os.path.exists(command_path):
      info['cmdline'] = shlex.split(
          osutils.ReadFile(command_path).strip())

    cwd_path = os.path.join(self._log_dir, 'ninja_cwd')
    if os.path.exists(cwd_path):
      info['cwd'] = osutils.ReadFile(cwd_path).strip()

    exit_path = os.path.join(self._log_dir, 'ninja_exit')
    if os.path.exists(exit_path):
      info['exit'] = int(osutils.ReadFile(exit_path).strip())

    env_path = os.path.join(self._log_dir, 'ninja_env')
    if os.path.exists(env_path):
      # env is null-byte separated, and has a trailing null byte.
      content = osutils.ReadFile(env_path).rstrip('\0')
      info['env'] = dict(line.split('=', 1) for line in content.split('\0'))

    if compiler_proxy_path:
      info['compiler_proxy_info'] = os.path.basename(compiler_proxy_path)

    return info
