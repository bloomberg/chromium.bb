# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generic utilities for all python scripts."""

import os
import signal
import subprocess
import sys
import urllib


def GetFileURLForPath(path):
  """Get file:// url for the given path.
  Also quotes the url using urllib.quote().
  """
  abs_path = os.path.abspath(path)
  if sys.platform == 'win32':
    # Don't quote the ':' in drive letter ( say, C: ) on win.
    # Also, replace '\' with '/' as expected in a file:/// url.
    drive, rest = os.path.splitdrive(abs_path)
    quoted_path = drive.upper() + urllib.quote((rest.replace('\\', '/')))
    return 'file:///' + quoted_path
  else:
    quoted_path = urllib.quote(abs_path)
    return 'file://' + quoted_path


def IsWin():
  return sys.platform == 'cygwin' or sys.platform.startswith('win')


def IsLinux():
  return sys.platform.startswith('linux')


def IsMac():
  return sys.platform.startswith('darwin')


def Kill(pid):
  """Terminate the given pid."""
  if IsWin():
    subprocess.call(['taskkill.exe', '/T', '/F', '/PID', str(pid)])
  else:
    os.kill(pid, signal.SIGTERM)
