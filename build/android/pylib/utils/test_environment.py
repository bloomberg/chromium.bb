# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import psutil

from pylib import android_commands

def _KillWebServers():
  for retry in xrange(5):
    for server in ['lighttpd', 'web-page-replay']:
      pids = [p.pid for p in psutil.process_iter() if server in p.name]
      for pid in pids:
        try:
          logging.warning('Killing %s %s', server, pid)
          os.kill(pid, signal.SIGQUIT)
        except Exception as e:
          logging.warning('Failed killing %s %s %s', server, pid, e)


def CleanupLeftoverProcesses():
  """Clean up the test environment, restarting fresh adb and HTTP daemons."""
  _KillWebServers()
  did_restart_host_adb = False
  for device in android_commands.GetAttachedDevices():
    adb = android_commands.AndroidCommands(device, api_strict_mode=True)
    # Make sure we restart the host adb server only once.
    if not did_restart_host_adb:
      adb.RestartAdbServer()
      did_restart_host_adb = True
    adb.RestartAdbdOnDevice()
    adb.EnableAdbRoot()
    adb.WaitForDevicePm()
