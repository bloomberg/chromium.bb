#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Start and stop Web Page Replay."""

import logging
import optparse
import os
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import urllib


USAGE = '%s [options] CHROME_EXE TEST_NAME' % os.path.basename(sys.argv[0])
USER_DATA_DIR = '{TEMP}/webpagereplay_utils-chrome'

# The port numbers must match those in chrome/test/perf/page_cycler_test.cc.
HTTP_PORT = 8080
HTTPS_PORT = 8413
REPLAY_HOST='127.0.0.1'


class ReplayError(Exception):
  """Catch-all exception for the module."""
  pass

class ReplayNotFoundError(ReplayError):
  pass

class ReplayNotStartedError(ReplayError):
  pass


class ReplayServer(object):
  """Start and Stop Web Page Replay.

  Example:
     with ReplayServer(replay_dir, archive_path, log_dir, replay_options):
       self.NavigateToURL(start_url)
       self.WaitUntil(...)
  """
  LOG_FILE = 'log.txt'

  def __init__(self, replay_dir, archive_path, log_dir, replay_options=None):
    """Initialize ReplayServer.

    Args:
      replay_dir: directory that has replay.py and related modules.
      archive_path: either a directory that contains WPR archives or,
          a path to a specific WPR archive.
      log_dir: where to write log.txt.
      replay_options: a list of options strings to forward to replay.py.
    """
    self.replay_dir = replay_dir
    self.archive_path = archive_path
    self.log_dir = log_dir
    self.replay_options = replay_options if replay_options else []

    self.log_name = os.path.join(self.log_dir, self.LOG_FILE)
    self.log_fh = None
    self.replay_process = None

    self.wpr_py = os.path.join(self.replay_dir, 'replay.py')
    if not os.path.exists(self.wpr_py):
      raise ReplayNotFoundError('Path does not exist: %s' % self.wpr_py)
    self.wpr_options = [
        '--port', str(HTTP_PORT),
        '--ssl_port', str(HTTPS_PORT),
        '--use_closest_match',
        # TODO(slamm): Add traffic shaping (requires root):
        # '--net', 'fios',
        ]
    self.wpr_options.extend(self.replay_options)

  def _OpenLogFile(self):
    if not os.path.exists(self.log_dir):
      os.makedirs(self.log_dir)
    return open(self.log_name, 'w')

  def IsStarted(self):
    """Checks to see if the server is up and running."""
    for _ in range(5):
      if self.replay_process.poll() is not None:
        # The process has exited.
        break
      try:
        up_url = '%s://localhost:%s/web-page-replay-generate-200'
        http_up_url = up_url % ('http', HTTP_PORT)
        https_up_url = up_url % ('https', HTTPS_PORT)
        if (200 == urllib.urlopen(http_up_url, None, {}).getcode() and
            200 == urllib.urlopen(https_up_url, None, {}).getcode()):
          return True
      except IOError:
        time.sleep(1)
    return False

  def StartServer(self):
    """Start Web Page Replay and verify that it started.

    Raises:
      ReplayNotStartedError if Replay start-up fails.
    """
    cmd_line = [self.wpr_py]
    cmd_line.extend(self.wpr_options)
    cmd_line.append(self.archive_path)
    self.log_fh = self._OpenLogFile()
    logging.debug('Starting Web-Page-Replay: %s', cmd_line)
    self.replay_process = subprocess.Popen(
      cmd_line, stdout=self.log_fh, stderr=subprocess.STDOUT)
    if not self.IsStarted():
      raise ReplayNotStartedError(
          'Web Page Replay failed to start. See the log file: ' + self.log_name)

  def StopServer(self):
    """Stop Web Page Replay."""
    if self.replay_process:
      logging.debug('Stopping Web-Page-Replay')
      # Use a SIGINT here so that it can do graceful cleanup.
      # Otherwise, we will leave subprocesses hanging.
      self.replay_process.send_signal(signal.SIGINT)
      self.replay_process.wait()
    if self.log_fh:
      self.log_fh.close()

  def __enter__(self):
    """Add support for with-statement."""
    self.StartServer()

  def __exit__(self, unused_exc_type, unused_exc_val, unused_exc_tb):
    """Add support for with-statement."""
    self.StopServer()
