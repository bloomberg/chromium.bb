#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for interacting with processes on a win32 system."""

import logging
import os
import pywintypes
import re
import subprocess
import sys
import time
import win32api
import win32com.client
import win32con
import win32event
import win32gui
import win32process

from kasko.util import ScopedStartStop


_DEFAULT_TIMEOUT = 10  # Seconds.
_LOGGER = logging.getLogger(os.path.basename(__file__))


def ImportSelenium(webdriver_dir=None):
  """Imports Selenium from the given path."""
  global webdriver
  global service
  if webdriver_dir:
    sys.path.append(webdriver_dir)
  from selenium import webdriver
  import selenium.webdriver.chrome.service as service


def FindChromeProcessId(user_data_dir, timeout=_DEFAULT_TIMEOUT):
  """Finds the process ID of a given Chrome instance."""
  udd = os.path.abspath(user_data_dir)

  # Find the message window.
  started = time.time()
  elapsed = 0
  msg_win = None
  while msg_win is None:
    try:
      win = win32gui.FindWindowEx(None, None, 'Chrome_MessageWindow', udd)
      if win != 0:
        msg_win = win
        break
    except pywintypes.error:
      continue

    time.sleep(0.1)
    elapsed = time.time() - started
    if elapsed >= timeout:
      raise TimeoutException()

  # Get the process ID associated with the message window.
  tid, pid = win32process.GetWindowThreadProcessId(msg_win)

  return pid


def ShutdownProcess(process_id, timeout, force=False):
  """Attempts to nicely close the specified process.

  Returns the exit code on success. Raises an error on failure.
  """

  # Open the process in question, so we can wait for it to exit.
  permissions = win32con.SYNCHRONIZE | win32con.PROCESS_QUERY_INFORMATION
  process_handle = win32api.OpenProcess(permissions, False, process_id)

  # Loop around to periodically retry to close Chrome.
  started = time.time()
  elapsed = 0
  while True:
    _LOGGER.debug('Shutting down process with PID=%d.', process_id)

    with open(os.devnull, 'w') as f:
      cmd = ['taskkill.exe', '/PID', str(process_id)]
      if force:
        cmd.append('/F')
      subprocess.call(cmd, shell=True, stdout=f, stderr=f)

    # Wait at most 2 seconds after each call to taskkill.
    curr_timeout_ms = int(max(2, timeout - elapsed) * 1000)

    _LOGGER.debug('Waiting for process with PID=%d to exit.', process_id)
    result = win32event.WaitForSingleObject(process_handle, curr_timeout_ms)
    # Exit the loop on successful wait.
    if result == win32event.WAIT_OBJECT_0:
      break

    elapsed = time.time() - started
    if elapsed > timeout:
      _LOGGER.debug('Timeout waiting for process to exit.')
      raise TimeoutException()

  exit_status = win32process.GetExitCodeProcess(process_handle)
  process_handle.Close()
  _LOGGER.debug('Process exited with status %d.', exit_status)

  return exit_status


def _WmiTimeToLocalEpoch(wmitime):
  """Converts a WMI time string to a Unix epoch time."""
  # The format of WMI times is: yyyymmddHHMMSS.xxxxxx[+-]UUU, where
  # UUU is the number of minutes between local time and UTC.
  m = re.match('^(?P<year>\d{4})(?P<month>\d{2})(?P<day>\d{2})'
                   '(?P<hour>\d{2})(?P<minutes>\d{2})(?P<seconds>\d{2}\.\d+)'
                   '(?P<offset>[+-]\d{3})$', wmitime)
  if not m:
    raise Exception('Invalid WMI time string.')

  # This parses the time as a local time.
  t = time.mktime(time.strptime(wmitime[0:14], '%Y%m%d%H%M%S'))

  # Add the fractional part of the seconds that wasn't parsed by strptime.
  s = float(m.group('seconds'))
  t += s - int(s)

  return t


def GetProcessCreationDate(pid):
  """Returns the process creation date as local unix epoch time."""
  wmi = win32com.client.GetObject('winmgmts:')
  procs = wmi.ExecQuery(
      'select CreationDate from Win32_Process where ProcessId = %s' % pid)
  for proc in procs:
    return _WmiTimeToLocalEpoch(proc.Properties_('CreationDate').Value)
  raise Exception('Unable to find process with PID %d.' % pid)


def ShutdownChildren(parent_pid, child_exe, started_after, started_before,
                     timeout=_DEFAULT_TIMEOUT, force=False):
  """Shuts down any lingering child processes of a given parent.

  This is an inherently racy thing to do as process IDs are aggressively reused
  on Windows. Filtering by a valid known |started_after| and |started_before|
  timestamp, as well as by the executable of the child process resolves this
  issue. Ugh.
  """
  started = time.time()
  wmi = win32com.client.GetObject('winmgmts:')
  _LOGGER.debug('Shutting down lingering children processes.')
  for proc in wmi.InstancesOf('Win32_Process'):
    if proc.Properties_('ParentProcessId').Value != parent_pid:
      continue
    if proc.Properties_('ExecutablePath').Value != child_exe:
      continue
    t = _WmiTimeToLocalEpoch(proc.Properties_('CreationDate').Value)
    if t <= started_after or t >= started_before:
      continue
    pid = proc.Properties_('ProcessId').Value
    remaining = max(0, started + timeout - time.time())
    ShutdownProcess(pid, remaining, force=force)


class ChromeInstance(object):
  """A class encapsulating a running instance of Chrome for testing.

  The Chrome instance is controlled via chromedriver and Selenium."""

  def __init__(self, chromedriver, chrome, user_data_dir):
    self.chromedriver_ = os.path.abspath(chromedriver)
    self.chrome_ = os.path.abspath(chrome)
    self.user_data_dir_ = user_data_dir

  def start(self, timeout=_DEFAULT_TIMEOUT):
    capabilities = {
          'chromeOptions': {
            'args': [
              # This allows automated navigation to chrome:// URLs.
              '--enable-gpu-benchmarking',
              '--user-data-dir=%s' % self.user_data_dir_,
            ],
            'binary': self.chrome_,
          }
        }

    # Use a _ScopedStartStop helper so the service and driver clean themselves
    # up in case of any exceptions.
    _LOGGER.info('Starting chromedriver')
    with ScopedStartStop(service.Service(self.chromedriver_)) as \
        scoped_service:
      _LOGGER.info('Starting chrome')
      with ScopedStartStop(webdriver.Remote(scoped_service.service.service_url,
                                            capabilities),
                           start=lambda x: None, stop=lambda x: x.quit()) as \
          scoped_driver:
        self.pid_ = FindChromeProcessId(self.user_data_dir_, timeout)
        self.started_at_ = GetProcessCreationDate(self.pid_)
        _LOGGER.debug('Chrome launched.')
        self.driver_ = scoped_driver.release()
        self.service_ = scoped_service.release()


  def stop(self, timeout=_DEFAULT_TIMEOUT):
    started = time.time()
    self.driver_.quit()
    self.stopped_at_ = time.time()
    self.service_.stop()
    self.driver_ = None
    self.service = None

    # Ensure that any lingering children processes are torn down as well. This
    # is generally racy on Windows, but is gated based on parent process ID,
    # child executable, and start time of the child process. These criteria
    # ensure we don't go indiscriminately killing processes.
    remaining = max(0, started + timeout - time.time())
    ShutdownChildren(self.pid_, self.chrome_, self.started_at_,
                     self.stopped_at_, remaining, force=True)

  def navigate_to(self, url):
    """Navigates the running Chrome instance to the provided URL."""
    self.driver_.get(url)
