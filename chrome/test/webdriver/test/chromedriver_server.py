# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Represents a running ChromeDriver server."""

import logging
import os
import platform
import signal
import subprocess
import sys
import threading
import time
import urllib2


class ChromeDriverServer:
  """A running ChromeDriver server."""

  def __init__(self, process, port, url_base=None):
    """Waits for the ChromeDriver server to fully start.

    Args:
      process:   the server process
      port:      port that ChromeDriver is listening on
      url_base:  url base for receiving webdriver commands
    Raises:
      RuntimeError if ChromeDriver does not start
    """
    self._process = process
    self._port = port
    self._url_base = url_base
    maxTime = time.time() + 30
    while time.time() < maxTime and not self.IsRunning():
      time.sleep(0.2)
    if not self.IsRunning():
      raise RuntimeError('ChromeDriver server did not start')

  def IsRunning(self):
    """Returns whether the server is up and running."""
    try:
      urllib2.urlopen(self.GetUrl() + '/status')
      return True
    except urllib2.URLError:
      return False

  def Kill(self):
    """Kills the ChromeDriver server, if it is running."""
    def _WaitForShutdown(process, shutdown_event):
      """Waits for the process to quit and then notifies."""
      process.wait()
      shutdown_event.acquire()
      shutdown_event.notify()
      shutdown_event.release()

    if self._process is None:
      return
    try:
      urllib2.urlopen(self.GetUrl() + '/shutdown').close()
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

  def GetUrl(self):
    url = 'http://localhost:' + str(self._port)
    if self._url_base:
      url += self._url_base
    return url

  def GetPort(self):
    return self._port
