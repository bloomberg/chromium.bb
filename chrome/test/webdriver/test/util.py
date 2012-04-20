# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generic utilities for all python scripts."""

import atexit
import os
import signal
import stat
import subprocess
import sys
import tempfile
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


def MakeTempDir(parent_dir=None):
  """Creates a temporary directory and returns an absolute path to it.

  The temporary directory is automatically deleted when the python interpreter
  exits normally.

  Args:
    parent_dir: the directory to create the temp dir in. If None, the system
                temp dir is used.

  Returns:
    The absolute path to the temporary directory.
  """
  path = tempfile.mkdtemp(dir=parent_dir)
  def DeleteDir():
    # Don't use shutil.rmtree because it can't delete read-only files on Win.
    for root, dirs, files in os.walk(path, topdown=False):
      for name in files:
        filename = os.path.join(root, name)
        os.chmod(filename, stat.S_IWRITE)
        os.remove(filename)
      for name in dirs:
        os.rmdir(os.path.join(root, name))
  atexit.register(DeleteDir)
  return path


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


def RunCommand(cmd, cwd=None):
  """Runs the given command and returns the exit code.

  Args:
    cmd: list of command arguments.
    cwd: working directory to execute the command, or None if the current
         working directory should be used.

  Returns:
    The exit code of the command.
  """
  process = subprocess.Popen(cmd, stdout=sys.stdout, stderr=sys.stderr, cwd=cwd)
  process.wait()
  return process.returncode


def Unzip(zip_path, output_dir):
  """Unzips the given zip file using a system installed unzip tool.

  Args:
    zip_path: zip file to unzip.
    output_dir: directory to unzip the contents of the zip file. The directory
                must exist.

  Raises:
    RuntimeError if the unzip operation fails.
  """
  if IsWin():
    unzip_cmd = ['C:\\Program Files\\7-Zip\\7z.exe', 'x', '-y']
  else:
    unzip_cmd = ['unzip', '-o']
  unzip_cmd += [zip_path]
  if RunCommand(unzip_cmd, output_dir) != 0:
    raise RuntimeError('Unable to unzip %s to %s' % (zip_path, output_dir))
