#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convience stand-alone file operations."""


import os
import shutil


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
  for p in paths:
    np = os.path.abspath(os.path.join(p, command))
    if os.path.isfile(np) and os.access(np, os.X_OK):
      return np
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
  if os.path.exists(path):
    shutil.rmtree(path)
