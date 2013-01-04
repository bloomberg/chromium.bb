#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Start and stop Web Page Replay.

Of the public module names, the following one is key:
  ReplayServer: a class to start/stop Web Page Replay.
"""

import logging
import os
import signal
import subprocess
import sys
import time
import urllib


_CHROME_BASE_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir, os.pardir, os.pardir))
REPLAY_DIR = os.path.join(
    _CHROME_BASE_DIR, 'src', 'third_party', 'webpagereplay')
LOG_PATH = os.path.join(
    _CHROME_BASE_DIR, 'src', 'webpagereplay_logs', 'logs.txt')


# Chrome options to make it work with Web Page Replay.
def GetChromeFlags(replay_host, http_port, https_port):
  return [
      '--host-resolver-rules=MAP * %s,EXCLUDE localhost' % replay_host,
      '--testing-fixed-http-port=%s' % http_port,
      '--testing-fixed-https-port=%s' % https_port,
      '--ignore-certificate-errors',
      ]

class ReplayError(Exception):
  """Catch-all exception for the module."""
  pass

class ReplayNotFoundError(ReplayError):
  def __init__(self, label, path):
    self.args = (label, path)

  def __str__(self):
    label, path = self.args
    return 'Path does not exist for %s: %s' % (label, path)

class ReplayNotStartedError(ReplayError):
  pass


class ReplayServer(object):
  """Start and Stop Web Page Replay.

  Web Page Replay is a proxy that can record and "replay" web pages with
  simulated network characteristics -- without having to edit the pages
  by hand. With WPR, tests can use "real" web content, and catch
  performance issues that may result from introducing network delays and
  bandwidth throttling.

  Example:
     with ReplayServer(archive_path):
       self.NavigateToURL(start_url)
       self.WaitUntil(...)

  Environment Variables (for development):
    WPR_ARCHIVE_PATH: path to alternate archive file (e.g. '/tmp/foo.wpr').
    WPR_RECORD: if set, puts Web Page Replay in record mode instead of replay.
    WPR_REPLAY_DIR: path to alternate Web Page Replay source.
  """
  def __init__(self, archive_path, replay_host, http_port, https_port,
               replay_options=None, replay_dir=None,
               log_path=None):
    """Initialize ReplayServer.

    Args:
      archive_path: a path to a specific WPR archive (required).
      replay_options: an iterable of options strings to forward to replay.py.
      replay_dir: directory that has replay.py and related modules.
      log_path: a path to a log file.
   """
    self.archive_path = os.environ.get('WPR_ARCHIVE_PATH', archive_path)
    self.replay_options = list(replay_options or ())
    self.replay_dir = os.environ.get('WPR_REPLAY_DIR', replay_dir or REPLAY_DIR)
    self.log_path = log_path or LOG_PATH
    self._http_port = http_port
    self._https_port = https_port
    self._replay_host = replay_host

    if 'WPR_RECORD' in os.environ and '--record' not in self.replay_options:
      self.replay_options.append('--record')
    self.is_record_mode = '--record' in self.replay_options
    self._AddDefaultReplayOptions()

    self.replay_py = os.path.join(self.replay_dir, 'replay.py')

    if self.is_record_mode:
      self._CheckPath('archive directory', os.path.dirname(self.archive_path))
    elif not os.path.exists(self.archive_path):
      self._CheckPath('archive file', self.archive_path)
    self._CheckPath('replay script', self.replay_py)

    self.log_fh = None
    self.replay_process = None

  def _AddDefaultReplayOptions(self):
    """Set WPR command-line options. Can be overridden if needed."""
    self.replay_options += [
        '--port', str(self._http_port),
        '--ssl_port', str(self._https_port),
        '--use_closest_match',
        '--no-dns_forwarding',
        ]

  def _CheckPath(self, label, path):
    if not os.path.exists(path):
      raise ReplayNotFoundError(label, path)

  def _OpenLogFile(self):
    log_dir = os.path.dirname(self.log_path)
    if not os.path.exists(log_dir):
      os.makedirs(log_dir)
    return open(self.log_path, 'w')

  def IsStarted(self):
    """Checks to see if the server is up and running."""
    for _ in range(5):
      if self.replay_process.poll() is not None:
        # The process has exited.
        break
      try:
        up_url = '%s://localhost:%s/web-page-replay-generate-200'
        http_up_url = up_url % ('http', self._http_port)
        https_up_url = up_url % ('https', self._https_port)
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
    cmd_line = [sys.executable, self.replay_py]
    cmd_line.extend(self.replay_options)
    cmd_line.append(self.archive_path)
    self.log_fh = self._OpenLogFile()
    logging.debug('Starting Web-Page-Replay: %s', cmd_line)
    self.replay_process = subprocess.Popen(
      cmd_line, stdout=self.log_fh, stderr=subprocess.STDOUT)
    if not self.IsStarted():
      log = open(self.log_path).read()
      raise ReplayNotStartedError(
          'Web Page Replay failed to start. Log output:\n%s' % log)

  def StopServer(self):
    """Stop Web Page Replay."""
    if self.replay_process:
      logging.debug('Stopping Web-Page-Replay')
      # Use a SIGINT so that it can do graceful cleanup. On Windows, we are left
      # with no other option than terminate().
      try:
        self.replay_process.send_signal(signal.SIGINT)
      except:
        self.replay_process.terminate()
      self.replay_process.wait()
    if self.log_fh:
      self.log_fh.close()

  def __enter__(self):
    """Add support for with-statement."""
    self.StartServer()
    return self

  def __exit__(self, unused_exc_type, unused_exc_val, unused_exc_tb):
    """Add support for with-statement."""
    self.StopServer()
