# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common file and os related utilities."""

import errno
import os

from chromite.lib import cros_build_lib


def WriteFile(path, content, mode='w', atomic=False):
  """Write the given content to disk.

  Args:
    path: Pathway to write the content to.
    content: Content to write.  May be either an iterable, or a string.
    mode: Optional; if binary mode is necessary, pass 'wb'.  If appending is
          desired, 'w+', etc.
    atomic: If the updating of the file should be done atomically.  Note this
          option is incompatible w/ append mode.
  """
  write_path = path
  if atomic:
    write_path = path + '.tmp'

  if isinstance(content, basestring):
    content = [content]

  with open(write_path, mode) as f:
    f.writelines(content)

  if not atomic:
    return

  try:
    os.rename(write_path, path)
  except EnvironmentError:
    SafeUnlink(write_path)
    raise


def ReadFile(path, mode='r'):
  """Read a given file on disk.  Primarily useful for one off small files."""
  with open(path, mode) as f:
    return f.read()


def SafeUnlink(path):
  """Unlink a file from disk, ignoring if it doesn't exist.

  Returns True if the file existed and was removed, False if it didn't exist.
  """
  try:
    os.unlink(path)
    return True
  except EnvironmentError, e:
    if e.errno != errno.ENOENT:
      raise
  return False


def SafeMakedirs(path, mode=0775, sudo=False):
  """Make parent directories if needed.  Ignore if existing.

  Arguments:
    path: The path to create.  Intermediate directories will be created as
          needed.
    mode: The access permissions in the style of chmod.
    sudo: If True, create it via sudo, thus root owned.
  Raises:
    EnvironmentError: if the makedir failed and it was non sudo.
    RunCommandError: If sudo mode, and the command failed for any reason.

  Returns:
    True if the directory had to be created, False if otherwise.
  """
  if sudo:
    if os.path.isdir(path):
      return False
    cros_build_lib.SudoRunCommand(
        ['mkdir', '-p', '--mode', oct(mode), path], print_cmd=False,
        redirect_stderr=True, redirect_stdout=True)
    return True

  try:
    os.makedirs(path, mode)
    return True
  except EnvironmentError, e:
    if e.errno != errno.EEXIST or not os.path.isdir(path):
      raise

  return False
