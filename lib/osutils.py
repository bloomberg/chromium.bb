# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common file and os related utilities, including tempdir manipulation."""

import contextlib
import errno
import os
import shutil
import tempfile
from chromite.lib import cros_build_lib

# Env vars that tempdir can be gotten from; minimally, this
# needs to match python's tempfile module and match normal
# unix standards.
_TEMPDIR_ENV_VARS = ('TMPDIR', 'TEMP', 'TMP')


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
def _TempDirSetup(self, prefix='tmp', update_env=True):
  """Generate a tempdir, modifying the object, and env to use it.

  Specifically, if update_env is True, then from this invocation forward,
  python and all subprocesses will use this location for their tempdir.

  The matching _TempDirTearDown restores the env to what it was.
  """
  # Stash the old tempdir that was used so we can
  # switch it back on the way out.
  self.tempdir = tempfile.mkdtemp(prefix=prefix)
  os.chmod(self.tempdir, 0700)

  if update_env:
    with tempfile._once_lock:
      self._tempdir_value = tempfile._get_default_tempdir()
      self._tempdir_env = tuple((x, os.environ.get(x))
                                for x in _TEMPDIR_ENV_VARS)
      # Now update TMPDIR/TEMP/TMP, and poke the python
      # internal to ensure all subprocess/raw tempfile
      # access goes into this location.
      os.environ.update((x, self.tempdir) for x in _TEMPDIR_ENV_VARS)
      # Finally, adjust python's cached value (we now it's cached by here
      # since we invoked _get_default_tempdir from above).  Note this
      # is necessary since we want *all* output from that point
      # forward to got to this location.
      tempfile.tempdir = self.tempdir


# pylint: disable=W0212,R0904,W0702
def _TempDirTearDown(self):
  # Note that _TempDirSetup may have failed, resulting in these attributes
  # not being set; this is why we use getattr here (and must).
  tempdir = getattr(self, 'tempdir', None)
  try:
    if tempdir is not None:
      shutil.rmtree(tempdir)
  except EnvironmentError, e:
    # Suppress ENOENT since we may be invoked
    # in a context where parallel wipes of the tempdir
    # may be occuring; primarily during hard shutdowns.
    if e.errno != errno.ENOENT:
      raise

  # Restore environment modification if necessary.
  tempdir_value = getattr(self, '_tempdir_value', None)
  if tempdir_value is not None:
    with tempfile._once_lock:
      tempfile.tempdir = self._tempdir_value
      for key, value in self._tempdir_env:
        if value is None:
          os.environ.pop(key, None)
        else:
          os.environ[key] = value


@contextlib.contextmanager
def TempDirContextManager(prefix=None, storage=None):
  """ContextManager constraining all tempfile/TMPDIR activity to a tempdir"""
  if storage is None:
    # Fake up a mutable object.
    # TOOD(build): rebase this to EasyAttr from cros_test_lib once it moves
    # to an importable location.
    class foon(object):
      pass
    storage = foon()

  _TempDirSetup(storage, prefix=prefix)
  try:
    yield
  finally:
    _TempDirTearDown(storage)

# pylint: disable=W0212,R0904,W0702
def TempDirDecorator(func):
  """Populates self.tempdir with path to a temporary writeable directory."""
  def f(self, *args, **kwargs):
    with TempDirContextManager(storage=self):
      return func(self, *args, **kwargs)

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
