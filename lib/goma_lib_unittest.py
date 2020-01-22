# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for goma_lib.py"""

from __future__ import print_function

import datetime
import getpass
import gzip
import json
import os
import time

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import goma_lib
from chromite.lib import osutils


class TestLogsArchiver(cros_test_lib.MockTempDirTestCase):
  """Tests for goma_lib.LogsArchiver."""

  def setUp(self):
    self.goma_log_dir = os.path.join(self.tempdir, 'goma_log_dir')
    osutils.SafeMakedirs(self.goma_log_dir)
    self.dest_dir = os.path.join(self.tempdir, 'destination_dir')
    osutils.SafeMakedirs(self.dest_dir)

  def _CreateFile(self, name):
    """Creates a basic file, such as a stats or counterz file.

    Args:
      name (str): Filename
    """
    osutils.WriteFile(os.path.join(self.goma_log_dir, name), 'File: ' + name)

  def _CreateLogFile(self, name, timestamp):
    """Creates a log file for testing.

    Args:
      name (str): Log file name.
      timestamp (datetime): timestamp that is written to the file.
    """
    path = os.path.join(
        self.goma_log_dir,
        '%s.host.log.INFO.%s' % (name, timestamp.strftime('%Y%m%d-%H%M%S.%f')))
    osutils.WriteFile(
        path,
        timestamp.strftime('Log file created at: %Y/%m/%d %H:%M:%S'))

  def testArchive(self):
    """Test successful archive of goma logs without stats/counterz specified."""
    self._CreateLogFile(
        'compiler_proxy', datetime.datetime(2017, 4, 26, 12, 0, 0))
    self._CreateLogFile(
        'compiler_proxy-subproc', datetime.datetime(2017, 4, 26, 12, 0, 0))
    self._CreateLogFile(
        'gomacc', datetime.datetime(2017, 4, 26, 12, 1, 0))
    self._CreateLogFile(
        'gomacc', datetime.datetime(2017, 4, 26, 12, 2, 0))

    archiver = goma_lib.LogsArchiver(self.goma_log_dir, dest_dir=self.dest_dir)
    archiver_tuple = archiver.Archive()

    archived_files = os.listdir(self.dest_dir)

    # Verify that the list of files returned matched what we find in the
    # dest_dir.
    self.assertCountEqual(archiver_tuple.log_files, archived_files)

    self.assertCountEqual(
        archived_files,
        ['compiler_proxy.host.log.INFO.20170426-120000.000000.gz',
         'gomacc.host.log.INFO.20170426-120100.000000.tar.gz',
         'compiler_proxy-subproc.host.log.INFO.20170426-120000.000000.gz'])

  def testArchiveWithStatsAndCounterz(self):
    """Test successful archive of goma logs with stats and counterz."""
    self._CreateLogFile(
        'compiler_proxy', datetime.datetime(2017, 4, 26, 12, 0, 0))
    self._CreateLogFile(
        'compiler_proxy-subproc', datetime.datetime(2017, 4, 26, 12, 0, 0))
    self._CreateLogFile(
        'gomacc', datetime.datetime(2017, 4, 26, 12, 1, 0))
    self._CreateLogFile(
        'gomacc', datetime.datetime(2017, 4, 26, 12, 2, 0))
    self._CreateFile('stats.binaryproto')
    self._CreateFile('counterz.binaryproto')

    archiver = goma_lib.LogsArchiver(self.goma_log_dir, dest_dir=self.dest_dir,
                                     stats_file='stats.binaryproto',
                                     counterz_file='counterz.binaryproto')
    archiver_tuple = archiver.Archive()

    self.assertCountEqual(
        archiver_tuple.log_files,
        ['compiler_proxy.host.log.INFO.20170426-120000.000000.gz',
         'gomacc.host.log.INFO.20170426-120100.000000.tar.gz',
         'compiler_proxy-subproc.host.log.INFO.20170426-120000.000000.gz'])
    self.assertEqual(archiver_tuple.stats_file, 'stats.binaryproto')
    self.assertEqual(archiver_tuple.counterz_file, 'counterz.binaryproto')

  def testArchiveWithStatsAndMissingCounterz(self):
    """Test successful archive of goma logs with stats and counterz."""
    self._CreateLogFile(
        'compiler_proxy', datetime.datetime(2017, 4, 26, 12, 0, 0))
    self._CreateLogFile(
        'compiler_proxy-subproc', datetime.datetime(2017, 4, 26, 12, 0, 0))
    self._CreateLogFile(
        'gomacc', datetime.datetime(2017, 4, 26, 12, 1, 0))
    self._CreateLogFile(
        'gomacc', datetime.datetime(2017, 4, 26, 12, 2, 0))
    self._CreateFile('stats.binaryproto')

    archiver = goma_lib.LogsArchiver(self.goma_log_dir, dest_dir=self.dest_dir,
                                     stats_file='stats.binaryproto',
                                     counterz_file='counterz.binaryproto')
    # Because counterz.binaryproto does not exist, verify it is not returned
    # as one of the files archived.
    archiver_tuple = archiver.Archive()
    self.assertFalse(archiver_tuple.counterz_file)
    self.assertEqual(archiver_tuple.stats_file, 'stats.binaryproto')
    self.assertCountEqual(
        archiver_tuple.log_files,
        ['compiler_proxy.host.log.INFO.20170426-120000.000000.gz',
         'gomacc.host.log.INFO.20170426-120100.000000.tar.gz',
         'compiler_proxy-subproc.host.log.INFO.20170426-120000.000000.gz'])

  def testNinjaLogArchive(self):
    """Test successful archive of ninja logs."""
    self._CreateLogFile(
        'compiler_proxy', datetime.datetime(2017, 8, 21, 12, 0, 0))
    self._CreateLogFile(
        'compiler_proxy-subproc', datetime.datetime(2017, 8, 21, 12, 0, 0))

    ninja_log_path = os.path.join(self.goma_log_dir, 'ninja_log')
    osutils.WriteFile(ninja_log_path, 'Ninja Log Content\n')
    timestamp = datetime.datetime(2017, 8, 21, 12, 0, 0)
    mtime = time.mktime(timestamp.timetuple())
    os.utime(ninja_log_path, ((time.time(), mtime)))

    osutils.WriteFile(
        os.path.join(self.goma_log_dir, 'ninja_command'), 'ninja_command')
    osutils.WriteFile(
        os.path.join(self.goma_log_dir, 'ninja_cwd'), 'ninja_cwd')
    osutils.WriteFile(
        os.path.join(self.goma_log_dir, 'ninja_env'),
        'key1=value1\0key2=value2\0')
    osutils.WriteFile(
        os.path.join(self.goma_log_dir, 'ninja_exit'), '0')

    archiver = goma_lib.LogsArchiver(self.goma_log_dir, dest_dir=self.dest_dir)
    archiver.Archive()
    archived_files = os.listdir(self.dest_dir)

    username = getpass.getuser()
    pid = os.getpid()
    hostname = cros_build_lib.GetHostName()
    ninjalog_filename = 'ninja_log.%s.%s.20170821-120000.%d.gz' % (
        username, hostname, pid)
    self.assertCountEqual(
        archived_files,
        [ninjalog_filename,
         'compiler_proxy-subproc.host.log.INFO.20170821-120000.000000.gz',
         'compiler_proxy.host.log.INFO.20170821-120000.000000.gz'])

    # Verify content of ninja_log file.
    ninjalog_path = os.path.join(self.dest_dir, ninjalog_filename)

    with gzip.open(ninjalog_path, 'rt') as gzip_file:
      ninja_log_content = gzip_file.read()

    content, eof, metadata_json = ninja_log_content.split('\n', 3)
    self.assertEqual('Ninja Log Content', content)
    self.assertEqual('# end of ninja log', eof)
    metadata = json.loads(metadata_json)
    self.assertEqual(
        metadata,
        {
            'platform': 'chromeos',
            'cmdline': ['ninja_command'],
            'cwd': 'ninja_cwd',
            'exit': 0,
            'env': {'key1': 'value1', 'key2': 'value2'},
            'compiler_proxy_info':
            'compiler_proxy.host.log.INFO.20170821-120000.000000'
        })
