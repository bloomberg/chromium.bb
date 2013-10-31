#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convience stand-alone file operations."""


import os
import shutil
import sys
import tempfile


def AtomicWriteFile(data, filename):
  """Write a file atomically.

  NOTE: Not atomic on Windows!
  Args:
    data: String to write to the file.
    filename: Filename to write.
  """
  filename = os.path.abspath(filename)
  handle, temp_file = tempfile.mkstemp(
      prefix='atomic_write', suffix='.tmp',
      dir=os.path.dirname(filename))
  fh = os.fdopen(handle, 'wb')
  fh.write(data)
  fh.close()
  # Window's can't move into place atomically, delete first.
  if sys.platform in ['win32', 'cygwin']:
    try:
      os.remove(filename)
    except OSError:
      pass
  os.rename(temp_file, filename)


def WriteFile(data, filename):
  """Write a file in one step.

  Args:
    data: String to write to the file.
    filename: Filename to write.
  """
  fh = open(filename, 'wb')
  fh.write(data)
  fh.close()


def ReadFile(filename):
  """Read a file in one step.

  Args:
    filename: Filename to read.
  Returns:
    String containing complete file.
  """
  fh = open(filename, 'rb')
  data = fh.read()
  fh.close()
  return data


class ExecutableNotFound(Exception):
  pass


def Which(command, paths=None):
  """Find the absolute path of a command in the current PATH.

  Args:
    command: Command name to look for.
    paths: Optional paths to search.
  Returns:
    Absolute path of the command (first one found),
    or default to a bare command if nothing is found.
  """
  if paths is None:
    paths = os.environ.get('PATH', '').split(os.pathsep)
  exe_suffixes = ['']
  if sys.platform == 'win32':
    exe_suffixes += ['.exe']
  for p in paths:
    np = os.path.abspath(os.path.join(p, command))
    for suffix in exe_suffixes:
      full_path = np + suffix
      if os.path.isfile(full_path) and os.access(full_path, os.X_OK):
        return full_path
  raise ExecutableNotFound('Unable to find: ' + command)


def MakeDirectoryIfAbsent(path):
  """Create a directory if it doesn't already exist.

  Args:
    path: Directory to create.
  """
  if not os.path.exists(path):
    os.mkdir(path)


def RemoveDirectoryIfPresent(path):
  """Remove a directory if it exists.

  Args:
    path: Directory to remove.
  """

  # On Windows, attempts to remove read-only files get Error 5. This
  # error handler fixes the permissions and retries the removal.
  def onerror_readonly(func, path, exc_info):
    import stat
    if not os.access(path, os.W_OK):
      os.chmod(path, stat.S_IWUSR)
      func(path)

  if os.path.exists(path):
    shutil.rmtree(path, onerror=onerror_readonly)
