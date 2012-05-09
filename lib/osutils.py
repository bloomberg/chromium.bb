# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common file and os related utilities, including tempdir manipulation."""

import errno
import os
import shutil
import tempfile


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


# pylint: disable=W0212,R0904,W0702
def _TempDirSetup(self):
  self.tempdir = tempfile.mkdtemp()
  os.chmod(self.tempdir, 0700)


# pylint: disable=W0212,R0904,W0702
def _TempDirTearDown(self):
  tempdir = getattr(self, 'tempdir', None)
  if tempdir is not None and os.path.exists(tempdir):
    shutil.rmtree(tempdir)


# pylint: disable=W0212,R0904,W0702
def TempDirDecorator(func):
  """Populates self.tempdir with path to a temporary writeable directory."""
  def f(self, *args, **kwargs):
    try:
      _TempDirSetup(self)
      return func(self, *args, **kwargs)
    finally:
      _TempDirTearDown(self)

  f.__name__ = func.__name__
  f.__doc__ = func.__doc__
  f.__module__ = func.__module__
  return f


def TempFileDecorator(func):
  """Populates self.tempfile with path to a temporary writeable file"""
  def f(self, *args, **kwargs):
    with tempfile.NamedTemporaryFile(dir=self.tempdir, delete=False) as f:
      self.tempfile = f.name
    return func(self, *args, **kwargs)

  f.__name__ = func.__name__
  f.__doc__ = func.__doc__
  f.__module__ = func.__module__
  return TempDirDecorator(f)
