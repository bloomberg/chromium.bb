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
import signal
import subprocess
import sys
import threading
import urllib2


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
      if self._exe_path is None:
        raise RuntimeError('ChromeDriver exe could not be found in its default '
                           'location. Searched in following directories: ' +
                           ', '.join(self.DefaultExeLocations()))
    if self._root_path is None:
      self._root_path = '.'
    self._root_path = os.path.abspath(self._root_path)
    self._process = None

    if not os.path.exists(self._exe_path):
      raise RuntimeError('ChromeDriver exe not found at: ' + self._exe_path)

    os.environ['PATH'] = os.path.dirname(self._exe_path) + os.environ['PATH']
    self.Start()

  @staticmethod
  def DefaultExeLocations():
    """Returns the paths that are used to find the ChromeDriver executable.

    Returns:
      a list of directories that would be searched for the executable
    """
    script_dir = os.path.dirname(__file__)
    chrome_src = os.path.abspath(os.path.join(
        script_dir, os.pardir, os.pardir, os.pardir))
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
    return [os.getcwd()] + bin_dirs.get(sys.platform, [])

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

    for dir in ChromeDriverLauncher.DefaultExeLocations():
      path = os.path.join(dir, exe_name)
      if os.path.exists(path):
        return os.path.abspath(path)
    return None

  def Start(self):
    """Starts a new ChromeDriver process.

    Kills a previous one if it is still running.

    Raises:
      RuntimeError if ChromeDriver does not start
    """
    def _WaitForLaunchResult(stdout, started_event, launch_result):
      """Reads from the stdout of ChromeDriver and parses the launch result.

      Args:
        stdout:        handle to ChromeDriver's standard output
        started_event: condition variable to notify when the launch result
                       has been parsed
        launch_result: dictionary to add the result of this launch to
      """
      status_line = stdout.readline()
      started_event.acquire()
      try:
        launch_result['success'] = status_line.startswith('Started')
        launch_result['status_line'] = status_line
        if launch_result['success']:
          port_line = stdout.readline()
          launch_result['port'] = int(port_line.split('=')[1])
        started_event.notify()
      finally:
        started_event.release()

    if self._process is not None:
      self.Kill()

    chromedriver_args = [self._exe_path, '--root=%s' % self._root_path]
    if self._port is not None:
      chromedriver_args += ['--port=%d' % self._port]
    proc = subprocess.Popen(chromedriver_args,
                            stdout=subprocess.PIPE)
    if proc is None:
      raise RuntimeError('ChromeDriver cannot be started')
    self._process = proc

    # Wait for ChromeDriver to be initialized before returning.
    launch_result = {}
    started_event = threading.Condition()
    started_event.acquire()
    spawn_thread = threading.Thread(
        target=_WaitForLaunchResult,
        args=(proc.stdout, started_event, launch_result))
    spawn_thread.start()
    started_event.wait(20)
    timed_out = 'success' not in launch_result
    started_event.release()
    if timed_out:
      raise RuntimeError('ChromeDriver did not respond')
    elif not launch_result['success']:
      raise RuntimeError('ChromeDriver failed to launch: ' +
                         launch_result['status_line'])
    self._port = launch_result['port']
    logging.info('ChromeDriver running on port %s' % self._port)

  def Kill(self):
    """Kills a currently running ChromeDriver process, if it is running."""
    def _WaitForShutdown(process, shutdown_event):
      """Waits for the process to quit and then notifies."""
      process.wait()
      shutdown_event.acquire()
      shutdown_event.notify()
      shutdown_event.release()

    if self._process is None:
      return
    try:
      urllib2.urlopen(self.GetURL() + '/shutdown').close()
    except urllib2.URLError:
      # Could not shutdown. Kill.
      pid = self._process.pid
      if platform.system() == 'Windows':
        subprocess.call(['taskkill.exe', '/T', '/F', '/PID', str(pid)])
      else:
        os.kill(pid, signal.SIGTERM)

    # Wait for ChromeDriver process to exit before returning.
    # Even if we had to kill the process above, we still should call wait
    # to cleanup the zombie.
    shutdown_event = threading.Condition()
    shutdown_event.acquire()
    wait_thread = threading.Thread(
        target=_WaitForShutdown,
        args=(self._process, shutdown_event))
    wait_thread.start()
    shutdown_event.wait(10)
    shutdown_event.release()
    self._process = None

  def GetURL(self):
    return 'http://localhost:' + str(self._port)

  def GetPort(self):
    return self._port

  def __del__(self):
    self.Kill()
