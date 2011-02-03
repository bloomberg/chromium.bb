#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Launches and kills ChromeDriver.

For ChromeDriver documentation, refer to:
  http://dev.chromium.org/developers/testing/webdriver-for-chrome
"""

import logging
import os
import platform
import subprocess
import sys

if sys.version_info < (2,6):
  # Subprocess.Popen.kill is not available prior to 2.6.
  if platform.system() == 'Windows':
    import win32api
  else:
    import signal


class ChromeDriverLauncher:
  """Launches and kills the ChromeDriver process."""

  def __init__(self, exe_path=None, root_path=None, port=None):
    """Initializes a new launcher.

    Args:
      exe_path:  path to the ChromeDriver executable
      root_path: base path from which ChromeDriver webserver will serve files
      port:      port that ChromeDriver will listen on
    """
    self._exe_path = exe_path
    self._root_path = root_path
    self._port = port
    if self._exe_path is None:
      self._exe_path = ChromeDriverLauncher.LocateExe()
    if self._root_path is None:
      self._root_path = '.'
    if self._port is None:
      self._port = 9515
    self._root_path = os.path.abspath(self._root_path)
    self._process = None

    if not os.path.exists(self._exe_path):
      raise RuntimeError('ChromeDriver exe not found at: ' + self._exe_path)
    self.Start()

  @staticmethod
  def LocateExe():
    """Attempts to locate the ChromeDriver executable.

    This searches the current directory, then checks the appropriate build
    locations according to platform.

    Returns:
      absolute path to the ChromeDriver executable, or None if not found
    """
    exe_name = 'chromedriver'
    if platform.system() == 'Windows':
      exe_name += '.exe'
    if os.path.exists(exe_name):
      return os.path.abspath(exe_name)

    script_dir = os.path.dirname(__file__)
    chrome_src = os.path.join(script_dir, os.pardir, os.pardir, os.pardir)
    bin_dirs = {
      'linux2': [ os.path.join(chrome_src, 'out', 'Debug'),
                  os.path.join(chrome_src, 'sconsbuild', 'Debug'),
                  os.path.join(chrome_src, 'out', 'Release'),
                  os.path.join(chrome_src, 'sconsbuild', 'Release')],
      'darwin': [ os.path.join(chrome_src, 'xcodebuild', 'Debug'),
                  os.path.join(chrome_src, 'xcodebuild', 'Release')],
      'win32':  [ os.path.join(chrome_src, 'chrome', 'Debug'),
                  os.path.join(chrome_src, 'build', 'Debug'),
                  os.path.join(chrome_src, 'chrome', 'Release'),
                  os.path.join(chrome_src, 'build', 'Release')],
    }
    for dir in bin_dirs.get(sys.platform, []):
      path = os.path.join(dir, exe_name)
      if os.path.exists(path):
        return os.path.abspath(path)
    return None

  def Start(self):
    """Starts a new ChromeDriver process.

    Kills a previous one if it is still running.
    """
    if self._process is not None:
      self.Kill()
    proc = subprocess.Popen([self._exe_path,
                             '--port=%d' % self._port,
                             '--root="%s"' % self._root_path])
    if proc is None:
      raise RuntimeError('ChromeDriver cannot be started')
    logging.info('Started chromedriver at port %s' % self._port)
    self._process = proc

  def Kill(self):
    """Kills a currently running ChromeDriver process, if it is running."""
    if self._process is None:
      return
    if sys.version_info < (2,6):
      # From http://stackoverflow.com/questions/1064335
      if platform.system() == 'Windows':
        PROCESS_TERMINATE = 1
        handle = win32api.OpenProcess(PROCESS_TERMINATE, False,
                                      self._process.pid)
        win32api.TerminateProcess(handle, -1)
        win32api.CloseHandle(handle)
      else:
        os.kill(self._process.pid, signal.SIGKILL)
    else:
      self._process.kill()
    self._process = None

  def __del__(self):
    self.Kill()

  def GetURL(self):
    return 'http://localhost:' + str(self._port)

  def GetPort(self):
    return self._port
