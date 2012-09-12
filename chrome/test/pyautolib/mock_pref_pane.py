# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Mock pref pane for testing purpose on Mac."""

import Foundation
import os
import signal
import subprocess
import sys
import tempfile
import time


class MockPrefPane(object):
  """Mock Pref Pane to enable/disable/changepin without system prompt.

  This only applies to Mac.
  """

  def __init__(self):
    self._service_name = 'org.chromium.chromoting'
    self._real_user_id = os.getuid()
    self._config_file = os.path.join(tempfile.gettempdir(),
                                     '%s.json' % self._service_name)
    self._tool_script = '/Library/PrivilegedHelperTools/%s.me2me.sh' % \
        self._service_name

  def _GetJobPid(self):
    """Gets the org.chromium.chromoting job id."""
    process = subprocess.Popen(['launchctl', 'list'], stdout=subprocess.PIPE)
    pid = None
    for line in process.stdout:
      # Format is:
      #   12345  -  my.job       (if my.job is running, number is job's PID)
      #   -      0  my.other.job (if my.other.job is not running)
      fields = line.strip().split('\t')
      if fields[2] == self._service_name and fields[0] != "-":
        pid = fields[0]
        break
    process.wait()
    return pid

  def Enable(self):
    """Handles what pref pane does for enabling connection."""
    # Elevate privileges, otherwise tool_script executes with EUID != 0.
    os.setuid(0)
    subprocess.call([self._tool_script, '--enable'],
                    stdin=open(self._config_file))

    # Drop privileges, start the launchd job as the logged-in user.
    os.setuid(self._real_user_id)
    subprocess.call(['launchctl', 'start', self._service_name])

    # Starting a launchd job is an asynchronous operation that typically takes
    # a couple of seconds, so poll until the job has started.
    for _ in range(1, 10):
      if self._GetJobPid():
        print '*** org.chromium.chromoting is running ***'
        break
      time.sleep(2)

  def Disable(self):
    """Handles what pref pane does for disabling connection."""
    # Elevate privileges, otherwise tool_script executes with EUID != 0.
    os.setuid(0)
    subprocess.call([self._tool_script, '--disable'],
                    stdin=open(self._config_file))

    # Drop privileges, stop the launchd job as the logged-in user.
    os.setuid(self._real_user_id)
    subprocess.call(['launchctl', 'stop', self._service_name])

    # Stopping a launchd job is an asynchronous operation that typically takes
    # a couple of seconds, so poll until the job has stopped.
    for _ in range(1, 10):
      if not self._GetJobPid():
        print '*** org.chromium.chromoting is not running ***'
        break
      time.sleep(2)

  def ChangePin(self):
    """Handles what pref pane does for changing pin."""
    # Elevate privileges, otherwise tool_script executes with EUID != 0.
    os.setuid(0)
    subprocess.call([self._tool_script, '--save-config'],
                    stdin=open(self._config_file))

    # Drop privileges and send SIGHUP to org.chromium.chromoting
    os.setuid(self._real_user_id)
    os.kill(int(self._GetJobPid()), signal.SIGHUP)

  def NotifyWebapp(self):
    """Notifies the web app that pref pane operation is done."""
    notif_center = Foundation.NSDistributedNotificationCenter.defaultCenter()
    notif_center.postNotificationName_object_userInfo_(
      self._service_name + '.update_succeeded', None, None)


def Main():
  """Handles the mock pref pane actions."""
  assert sys.platform.startswith('darwin')

  print '*** Started mock pref pane ***'
  print '*** EUID=%d, UID=%d ***' % (os.geteuid(), os.getuid())

  pref_pane = MockPrefPane()

  if sys.argv[1] == 'enable':
    pref_pane.Enable()
  elif sys.argv[1] == 'disable':
    pref_pane.Disable()
  elif sys.argv[1] == 'changepin':
    pref_pane.ChangePin()
  else:
    print >>sys.stderr, 'Invalid syntax'
    return

  pref_pane.NotifyWebapp()


if __name__ == '__main__':
  Main()