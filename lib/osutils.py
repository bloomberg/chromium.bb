# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common file and os related utilities, including tempdir manipulation."""

from __future__ import print_function

import collections
import contextlib
import ctypes
import ctypes.util
import datetime
import errno
import glob
import hashlib
import os
import pwd
import re
import shutil
import stat
import subprocess
import tempfile

import six

from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import retry_util
from chromite.utils import key_value_store


# Env vars that tempdir can be gotten from; minimally, this
# needs to match python's tempfile module and match normal
# unix standards.
_TEMPDIR_ENV_VARS = ('TMPDIR', 'TEMP', 'TMP')


def GetNonRootUser():
  """Returns a non-root user. Defaults to the current user.

  If the current user is root, returns the username of the person who
  ran the emerge command. If running using sudo, returns the username
  of the person who ran the sudo command. If no non-root user is
  found, returns None.
  """
  uid = os.getuid()
  if uid == 0:
    user = os.environ.get('PORTAGE_USERNAME', os.environ.get('SUDO_USER'))
  else:
    user = pwd.getpwuid(os.getuid()).pw_name

  if user == 'root':
    return None
  else:
    return user


def IsChildProcess(pid, name=None):
  """Return True if pid is a child of the current process.

  Args:
    pid: Child pid to search for in current process's pstree.
    name: Name of the child process.

  Note:
    This function is not fool proof. If the process tree contains wierd names,
    an incorrect match might be possible.
  """
  cmd = ['pstree', '-Ap', str(os.getpid())]
  pstree = cros_build_lib.run(cmd, capture_output=True, print_cmd=False,
                              encoding='utf-8').stdout
  if name is None:
    match = '(%d)' % pid
  else:
    match = '-%s(%d)' % (name, pid)
  return match in pstree


def ExpandPath(path):
  """Returns path after passing through realpath and expanduser."""
  return os.path.realpath(os.path.expanduser(path))


def IsSubPath(path, other):
  """Returns whether |path| is a sub path of |other|."""
  path = os.path.abspath(path)
  other = os.path.abspath(other)
  if path == other:
    return True
  return path.startswith(other + os.sep)


def AllocateFile(path, size, makedirs=False):
  """Allocates a file of a certain |size| in |path|.

  Args:
    path: Path to allocate the file.
    size: The length, in bytes, of the desired file.
    makedirs: If True, create missing leading directories in the path.
  """
  if makedirs:
    SafeMakedirs(os.path.dirname(path))

  with open(path, 'w') as out:
    out.truncate(size)


# All the modes that we allow people to pass to WriteFile.  This allows us to
# make assumptions about the input so we can update it if needed.
_VALID_WRITE_MODES = {
    # Read & write, but no truncation, and file offset is 0.
    'r+', 'r+b',
    # Writing (and maybe reading) with truncation.
    'w', 'wb', 'w+', 'w+b',
    # Writing (and maybe reading), but no truncation, and file offset is at end.
    'a', 'ab', 'a+', 'a+b',
}


def WriteFile(path, content, mode='w', encoding=None, errors=None, atomic=False,
              makedirs=False, sudo=False):
  """Write the given content to disk.

  Args:
    path: Pathway to write the content to.
    content: Content to write.  May be either an iterable, or a string.
    mode: The mode to use when opening the file.  'w' is for text files (see the
      following settings) and 'wb' is for binary files.  If appending, pass
      'w+', etc...
    encoding: The encoding of the file content.  Text files default to 'utf-8'.
    errors: How to handle encoding errors.  Text files default to 'strict'.
    atomic: If the updating of the file should be done atomically.  Note this
            option is incompatible w/ append mode.
    makedirs: If True, create missing leading directories in the path.
    sudo: If True, write the file as root.
  """
  if mode not in _VALID_WRITE_MODES:
    raise ValueError('mode must be one of {"%s"}, not %r' %
                     ('", "'.join(sorted(_VALID_WRITE_MODES)), mode))

  if sudo and atomic and ('a' in mode or '+' in mode):
    raise ValueError('append mode does not work in sudo+atomic mode')

  if 'b' in mode:
    if encoding is not None or errors is not None:
      raise ValueError('binary mode does not use encoding/errors')
  else:
    if encoding is None:
      encoding = 'utf-8'
    if errors is None:
      errors = 'strict'

  if makedirs:
    SafeMakedirs(os.path.dirname(path), sudo=sudo)

  # TODO(vapier): We can merge encoding/errors into the open call once we are
  # Python 3 only.  Until then, we have to handle it ourselves.
  if 'b' in mode:
    write_wrapper = lambda x: x
  else:
    mode += 'b'
    def write_wrapper(iterable):
      for item in iterable:
        yield item.encode(encoding, errors)

  # If the file needs to be written as root and we are not root, write to a temp
  # file, move it and change the permission.
  if sudo and os.getuid() != 0:
    if 'a' in mode or '+' in mode:
      # Use dd to run through sudo & append the output, and write the new data
      # to it through stdin.
      cros_build_lib.sudo_run(
          ['dd', 'conv=notrunc', 'oflag=append', 'status=none',
           'of=%s' % (path,)], print_cmd=False, input=content)

    else:
      with tempfile.NamedTemporaryFile(mode=mode, delete=False) as temp:
        write_path = temp.name
        temp.writelines(write_wrapper(
            cros_build_lib.iflatten_instance(content)))
      os.chmod(write_path, 0o644)

      try:
        mv_target = path if not atomic else path + '.tmp'
        cros_build_lib.sudo_run(['mv', write_path, mv_target],
                                print_cmd=False, stderr=True)
        Chown(mv_target, user='root', group='root')
        if atomic:
          cros_build_lib.sudo_run(['mv', mv_target, path],
                                  print_cmd=False, stderr=True)

      except cros_build_lib.RunCommandError:
        SafeUnlink(write_path)
        SafeUnlink(mv_target)
        raise

  else:
    # We have the right permissions, simply write the file in python.
    write_path = path
    if atomic:
      write_path = path + '.tmp'
    with open(write_path, mode) as f:
      f.writelines(write_wrapper(cros_build_lib.iflatten_instance(content)))

    if not atomic:
      return

    try:
      os.rename(write_path, path)
    except EnvironmentError:
      SafeUnlink(write_path)
      raise


def Touch(path, makedirs=False, mode=None):
  """Simulate unix touch. Create if doesn't exist and update its timestamp.

  Args:
    path: a string, file name of the file to touch (creating if not present).
    makedirs: If True, create missing leading directories in the path.
    mode: The access permissions to set.  In the style of chmod.  Defaults to
          using the umask.
  """
  if makedirs:
    SafeMakedirs(os.path.dirname(path))

  # Create the file if nonexistant.
  open(path, 'a').close()
  if mode is not None:
    os.chmod(path, mode)
  # Update timestamp to right now.
  os.utime(path, None)


def Chown(path, user=None, group=None, recursive=False):
  """Simple sudo chown path to the user.

  Defaults to user running command. Does nothing if run as root user unless
  a new owner is provided.

  Args:
    path: str - File/directory to chown.
    user: str|int|None - User to chown the file to. Defaults to current user.
    group: str|int|None - Group to assign the file to.
    recursive: Also chown child files/directories recursively.
  """
  if user is None:
    user = GetNonRootUser() or ''
  else:
    user = str(user)

  group = '' if group is None else str(group)

  if user or group:
    cmd = ['chown']
    if recursive:
      cmd += ['-R']
    cmd += ['%s:%s' % (user, group), path]
    cros_build_lib.sudo_run(cmd, print_cmd=False,
                            stderr=True, stdout=True)


def ReadFile(path, mode='r', encoding=None, errors=None):
  """Read a given file on disk.  Primarily useful for one off small files.

  The defaults are geared towards reading UTF-8 encoded text.

  Args:
    path: The file to read.
    mode: The mode to use when opening the file.  'r' is for text files (see the
      following settings) and 'rb' is for binary files.
    encoding: The encoding of the file content.  Text files default to 'utf-8'.
    errors: How to handle encoding errors.  Text files default to 'strict'.

  Returns:
    The content of the file, either as bytes or a string (with the specified
    encoding).
  """
  if mode not in ('r', 'rb'):
    raise ValueError('mode may only be "r" or "rb", not %r' % (mode,))

  if 'b' in mode:
    if encoding is not None or errors is not None:
      raise ValueError('binary mode does not use encoding/errors')
  else:
    if encoding is None:
      encoding = 'utf-8'
    if errors is None:
      errors = 'strict'

  with open(path, 'rb') as f:
    # TODO(vapier): We can merge encoding/errors into the open call once we are
    # Python 3 only.  Until then, we have to handle it ourselves.
    ret = f.read()
    if 'b' not in mode:
      ret = ret.decode(encoding, errors)
    return ret


def MD5HashFile(path):
  """Calculate the md5 hash of a given file path.

  Args:
    path: The path of the file to hash.

  Returns:
    The hex digest of the md5 hash of the file.
  """
  contents = ReadFile(path, mode='rb')
  return hashlib.md5(contents).hexdigest()


def SafeSymlink(source, dest, sudo=False):
  """Create a symlink at |dest| pointing to |source|.

  This will override the |dest| if the symlink exists. This operation is not
  atomic.

  Args:
    source: source path.
    dest: destination path.
    sudo: If True, create the link as root.
  """
  if sudo and os.getuid() != 0:
    cros_build_lib.sudo_run(['ln', '-sfT', source, dest],
                            print_cmd=False, stderr=True)
  else:
    SafeUnlink(dest)
    os.symlink(source, dest)


def SafeUnlink(path, sudo=False):
  """Unlink a file from disk, ignoring if it doesn't exist.

  Returns:
    True if the file existed and was removed, False if it didn't exist.
  """
  try:
    os.unlink(path)
    return True
  except EnvironmentError as e:
    if e.errno == errno.ENOENT:
      return False

    if not sudo:
      raise

  # If we're still here, we're falling back to sudo.
  cros_build_lib.sudo_run(['rm', '--', path], print_cmd=False, stderr=True)
  return True


def SafeMakedirs(path, mode=0o775, sudo=False, user='root'):
  """Make parent directories if needed.  Ignore if existing.

  Args:
    path: The path to create.  Intermediate directories will be created as
          needed.
    mode: The access permissions in the style of chmod.
    sudo: If True, create it via sudo, thus root owned.
    user: If |sudo| is True, run sudo as |user|.

  Returns:
    True if the directory had to be created, False if otherwise.

  Raises:
    EnvironmentError: If the makedir failed.
    RunCommandError: If using run and the command failed for any reason.
  """
  if sudo and not (os.getuid() == 0 and user == 'root'):
    if os.path.isdir(path):
      return False
    cros_build_lib.sudo_run(
        ['mkdir', '-p', '--mode', '%o' % mode, path], user=user,
        print_cmd=False, stderr=True, stdout=True)
    cros_build_lib.sudo_run(
        ['chmod', '%o' % mode, path],
        print_cmd=False, stderr=True, stdout=True)
    return True

  try:
    os.makedirs(path, mode)
    # If we made the directory, force the mode.
    os.chmod(path, mode)
    return True
  except EnvironmentError as e:
    if e.errno != errno.EEXIST or not os.path.isdir(path):
      raise

  # If the mode on the directory does not match the request, log it.
  # It is the callers responsibility to coordinate mode values if there is a
  # need for that.
  if stat.S_IMODE(os.stat(path).st_mode) != mode:
    try:
      os.chmod(path, mode)
    except EnvironmentError:
      # Just make sure it's a directory.
      if not os.path.isdir(path):
        raise
  return False


class MakingDirsAsRoot(Exception):
  """Raised when creating directories as root."""


def SafeMakedirsNonRoot(path, mode=0o775, user=None):
  """Create directories and make sure they are not owned by root.

  See SafeMakedirs for the arguments and returns.
  """
  if user is None:
    user = GetNonRootUser()

  if user is None or user == 'root':
    raise MakingDirsAsRoot('Refusing to create %s as user %s!' % (path, user))

  created = False
  should_chown = False
  try:
    created = SafeMakedirs(path, mode=mode, user=user)
    if not created:
      # Sometimes, the directory exists, but is owned by root. As a HACK, we
      # will chown it to the requested user.
      stat_info = os.stat(path)
      should_chown = (stat_info.st_uid == 0)
  except OSError as e:
    if e.errno == errno.EACCES:
      # Sometimes, (a prefix of the) path we're making the directory in may be
      # owned by root, and so we fail. As a HACK, use da power to create
      # directory and then chown it.
      created = should_chown = SafeMakedirs(path, mode=mode, sudo=True)

  if should_chown:
    Chown(path, user=user)

  return created


class BadPathsException(Exception):
  """Raised by various osutils path manipulation functions on bad input."""


def CopyDirContents(from_dir, to_dir, symlinks=False, allow_nonempty=False):
  """Copy contents of from_dir to to_dir. Both should exist.

  shutil.copytree allows one to copy a rooted directory tree along with the
  containing directory. OTOH, this function copies the contents of from_dir to
  an existing directory. For example, for the given paths:

  from/
    inside/x.py
    y.py
  to/

  shutil.copytree('from', 'to')
  # Raises because 'to' already exists.

  shutil.copytree('from', 'to/non_existent_dir')
  to/non_existent_dir/
    inside/x.py
    y.py

  CopyDirContents('from', 'to')
  to/
    inside/x.py
    y.py

  Args:
    from_dir: The directory whose contents should be copied. Must exist.
    to_dir: The directory to which contents should be copied. Must exist.
    symlinks: Whether symlinks should be copied or dereferenced. When True, all
        symlinks will be copied as symlinks into the destination. When False,
        the symlinks will be dereferenced and the contents copied over.
    allow_nonempty: If True, do not die when to_dir is nonempty.

  Raises:
    BadPathsException: if the source / target directories don't exist, or if
        target directory is non-empty when allow_nonempty=False.
    OSError: on esoteric permission errors.
  """
  if not os.path.isdir(from_dir):
    raise BadPathsException('Source directory %s does not exist.' % from_dir)
  if not os.path.isdir(to_dir):
    raise BadPathsException('Destination directory %s does not exist.' % to_dir)
  if os.listdir(to_dir) and not allow_nonempty:
    raise BadPathsException('Destination directory %s is not empty.' % to_dir)

  for name in os.listdir(from_dir):
    from_path = os.path.join(from_dir, name)
    to_path = os.path.join(to_dir, name)
    if symlinks and os.path.islink(from_path):
      os.symlink(os.readlink(from_path), to_path)
    elif os.path.isdir(from_path):
      shutil.copytree(from_path, to_path, symlinks=symlinks)
    elif os.path.isfile(from_path):
      shutil.copy2(from_path, to_path)


def RmDir(path, ignore_missing=False, sudo=False):
  """Recursively remove a directory.

  Args:
    path: Path of directory to remove.
    ignore_missing: Do not error when path does not exist.
    sudo: Remove directories as root.
  """
  # Using `sudo` is a bit expensive, so try to delete everything natively first.
  try:
    shutil.rmtree(path)
    return
  except EnvironmentError as e:
    if ignore_missing and e.errno == errno.ENOENT:
      return

    if not sudo:
      raise

  # If we're still here, we're falling back to sudo.
  try:
    cros_build_lib.sudo_run(
        ['rm', '-r%s' % ('f' if ignore_missing else '',), '--', path],
        debug_level=logging.DEBUG, stdout=True, stderr=True)
  except cros_build_lib.RunCommandError as e:
    if not ignore_missing or os.path.exists(path):
      # If we're not ignoring the rm ENOENT equivalent, throw it;
      # if the pathway still exists, something failed, thus throw it.
      raise


class EmptyDirNonExistentException(BadPathsException):
  """EmptyDir was called on a non-existent directory without ignore_missing."""


def EmptyDir(path, ignore_missing=False, sudo=False, exclude=()):
  """Remove all files inside a directory, including subdirs.

  Args:
    path: Path of directory to empty.
    ignore_missing: Do not error when path does not exist.
    sudo: Remove directories as root.
    exclude: Iterable of file names to exclude from the cleanup. They should
             exactly match the file or directory name in path.
             e.g. ['foo', 'bar']

  Raises:
    EmptyDirNonExistentException: if ignore_missing false, and dir is missing.
    OSError: If the directory is not user writable.
  """
  path = ExpandPath(path)
  exclude = set(exclude)

  if not os.path.exists(path):
    if ignore_missing:
      return
    raise EmptyDirNonExistentException(
        'EmptyDir called non-existent: %s' % path)

  # We don't catch OSError if path is not a directory.
  for candidate in os.listdir(path):
    if candidate not in exclude:
      subpath = os.path.join(path, candidate)
      # Both options can throw OSError if there is a permission problem.
      if os.path.isdir(subpath):
        RmDir(subpath, ignore_missing=ignore_missing, sudo=sudo)
      else:
        SafeUnlink(subpath, sudo)


def Which(binary, path=None, mode=os.X_OK, root=None):
  """Return the absolute path to the specified binary.

  Args:
    binary: The binary to look for.
    path: Search path. Defaults to os.environ['PATH'].
    mode: File mode to check on the binary.
    root: Path to automatically prefix to every element of |path|.

  Returns:
    The full path to |binary| if found (with the right mode). Otherwise, None.
  """
  if path is None:
    path = os.environ.get('PATH', '')
  for p in path.split(os.pathsep):
    if root and p.startswith('/'):
      # Don't prefix relative paths.  We might want to support this at some
      # point, but it's not worth the coding hassle currently.
      p = os.path.join(root, p.lstrip('/'))
    p = os.path.join(p, binary)
    if os.path.isfile(p) and os.access(p, mode):
      return p
  return None


def FindMissingBinaries(needed_tools):
  """Verifies that the required tools are present on the system.

  This is especially important for scripts that are intended to run
  outside the chroot.

  Args:
    needed_tools: an array of string specified binaries to look for.

  Returns:
    If all tools are found, returns the empty list. Otherwise, returns the
    list of missing tools.
  """
  return [binary for binary in needed_tools if Which(binary) is None]


def DirectoryIterator(base_path):
  """Iterates through the files and subdirs of a directory."""
  for root, dirs, files in os.walk(base_path):
    for e in [d + os.sep for d in dirs] + files:
      yield os.path.join(root, e)


def IteratePaths(end_path):
  """Generator that iterates down to |end_path| from root /.

  Args:
    end_path: The destination. If this is a relative path, it will be resolved
        to absolute path. In all cases, it will be normalized.

  Yields:
    All the paths gradually constructed from / to |end_path|. For example:
    IteratePaths("/this/path") yields "/", "/this", and "/this/path".
  """
  return reversed(list(IteratePathParents(end_path)))


def IteratePathParents(start_path):
  """Generator that iterates through a directory's parents.

  Args:
    start_path: The path to start from.

  Yields:
    The passed-in path, along with its parents.  i.e.,
    IteratePathParents('/usr/local') would yield '/usr/local', '/usr', and '/'.
  """
  path = os.path.abspath(start_path)
  # There's a bug that abspath('//') returns '//'. We need to renormalize it.
  if path == '//':
    path = '/'
  yield path
  while path.strip('/'):
    path = os.path.dirname(path)
    yield path


def FindInPathParents(path_to_find, start_path, test_func=None, end_path=None):
  """Look for a relative path, ascending through parent directories.

  Ascend through parent directories of current path looking for a relative
  path.  I.e., given a directory structure like:
  -/
   |
   --usr
     |
     --bin
     |
     --local
       |
       --google

  the call FindInPathParents('bin', '/usr/local') would return '/usr/bin', and
  the call FindInPathParents('google', '/usr/local') would return
  '/usr/local/google'.

  Args:
    path_to_find: The relative path to look for.
    start_path: The path to start the search from.  If |start_path| is a
      directory, it will be included in the directories that are searched.
    test_func: The function to use to verify the relative path.  Defaults to
      os.path.exists.  The function will be passed one argument - the target
      path to test.  A True return value will cause AscendingLookup to return
      the target.
    end_path: The path to stop searching.
  """
  if end_path is not None:
    end_path = os.path.abspath(end_path)
  if test_func is None:
    test_func = os.path.exists
  for path in IteratePathParents(start_path):
    if path == end_path:
      return None
    target = os.path.join(path, path_to_find)
    if test_func(target):
      return target
  return None


def SetGlobalTempDir(tempdir_value, tempdir_env=None):
  """Set the global temp directory to the specified |tempdir_value|

  Args:
    tempdir_value: The new location for the global temp directory.
    tempdir_env: Optional. A list of key/value pairs to set in the
      environment. If not provided, set all global tempdir environment
      variables to point at |tempdir_value|.

  Returns:
    Returns (old_tempdir_value, old_tempdir_env).

    old_tempdir_value: The old value of the global temp directory.
    old_tempdir_env: A list of the key/value pairs that control the tempdir
      environment and were set prior to this function. If the environment
      variable was not set, it is recorded as None.
  """
  # pylint: disable=protected-access
  with tempfile._once_lock:
    old_tempdir_value = GetGlobalTempDir()
    old_tempdir_env = tuple((x, os.environ.get(x)) for x in _TEMPDIR_ENV_VARS)

    # Now update TMPDIR/TEMP/TMP, and poke the python
    # internals to ensure all subprocess/raw tempfile
    # access goes into this location.
    if tempdir_env is None:
      os.environ.update((x, tempdir_value) for x in _TEMPDIR_ENV_VARS)
    else:
      for key, value in tempdir_env:
        if value is None:
          os.environ.pop(key, None)
        else:
          os.environ[key] = value

    # Finally, adjust python's cached value (we know it's cached by here
    # since we invoked _get_default_tempdir from above).  Note this
    # is necessary since we want *all* output from that point
    # forward to go to this location.
    tempfile.tempdir = tempdir_value

  return (old_tempdir_value, old_tempdir_env)


def GetGlobalTempDir():
  """Get the path to the current global tempdir.

  The global tempdir path can be modified through calls to SetGlobalTempDir.
  """
  # pylint: disable=protected-access
  return tempfile._get_default_tempdir()


def _TempDirSetup(self, prefix='tmp', set_global=False, base_dir=None):
  """Generate a tempdir, modifying the object, and env to use it.

  Specifically, if set_global is True, then from this invocation forward,
  python and all subprocesses will use this location for their tempdir.

  The matching _TempDirTearDown restores the env to what it was.
  """
  # Stash the old tempdir that was used so we can
  # switch it back on the way out.
  self.tempdir = tempfile.mkdtemp(prefix=prefix, dir=base_dir)
  os.chmod(self.tempdir, 0o700)

  if set_global:
    self._orig_tempdir_value, self._orig_tempdir_env = \
        SetGlobalTempDir(self.tempdir)


def _TempDirTearDown(self, force_sudo, delete=True):
  # Note that _TempDirSetup may have failed, resulting in these attributes
  # not being set; this is why we use getattr here (and must).
  tempdir = getattr(self, 'tempdir', None)
  try:
    if tempdir is not None and delete:
      RmDir(tempdir, ignore_missing=True, sudo=force_sudo)
  except EnvironmentError as e:
    # Suppress ENOENT since we may be invoked
    # in a context where parallel wipes of the tempdir
    # may be occuring; primarily during hard shutdowns.
    if e.errno != errno.ENOENT:
      raise

  # Restore environment modification if necessary.
  orig_tempdir_value = getattr(self, '_orig_tempdir_value', None)
  if orig_tempdir_value is not None:
    # pylint: disable=protected-access
    SetGlobalTempDir(orig_tempdir_value, self._orig_tempdir_env)


class TempDir(object):
  """Object that creates a temporary directory.

  This object can either be used as a context manager or just as a simple
  object. The temporary directory is stored as self.tempdir in the object, and
  is returned as a string by a 'with' statement.
  """

  def __init__(self, **kwargs):
    """Constructor. Creates the temporary directory.

    Args:
      prefix: See tempfile.mkdtemp documentation.
      base_dir: The directory to place the temporary directory.
      set_global: Set this directory as the global temporary directory.
      delete: Whether the temporary dir should be deleted as part of cleanup.
          (default: True)
      sudo_rm: Whether the temporary dir will need root privileges to remove.
          (default: False)
    """
    self.kwargs = kwargs.copy()
    self.delete = kwargs.pop('delete', True)
    self.sudo_rm = kwargs.pop('sudo_rm', False)
    self.tempdir = None
    _TempDirSetup(self, **kwargs)

  def SetSudoRm(self, enable=True):
    """Sets |sudo_rm|, which forces us to delete temporary files as root."""
    self.sudo_rm = enable

  def Cleanup(self):
    """Clean up the temporary directory."""
    if self.tempdir is not None:
      try:
        _TempDirTearDown(self, self.sudo_rm, delete=self.delete)
      finally:
        self.tempdir = None

  def __enter__(self):
    """Return the temporary directory."""
    return self.tempdir

  def __exit__(self, exc_type, exc_value, exc_traceback):
    try:
      self.Cleanup()
    except Exception:
      if exc_type:
        # If an exception from inside the context was already in progress,
        # log our cleanup exception, then allow the original to resume.
        logging.error('While exiting %s:', self, exc_info=True)

        if self.tempdir:
          # Log all files in tempdir at the time of the failure.
          try:
            logging.error('Directory contents were:')
            for name in os.listdir(self.tempdir):
              logging.error('  %s', name)
          except OSError:
            logging.error('  Directory did not exist.')

          # Log all mounts at the time of the failure, since that's the most
          # common cause.
          mount_results = cros_build_lib.run(
              ['mount'], stdout=True, stderr=subprocess.STDOUT,
              check=False)
          logging.error('Mounts were:')
          logging.error('  %s', mount_results.output)

      else:
        # If there was not an exception from the context, raise ours.
        raise

  def __del__(self):
    self.Cleanup()

  def __str__(self):
    return self.tempdir if self.tempdir else ''


def TempDirDecorator(func):
  """Populates self.tempdir with path to a temporary writeable directory."""
  def f(self, *args, **kwargs):
    with TempDir() as tempdir:
      self.tempdir = tempdir
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


# Flags synced from sys/mount.h.  See mount(2) for details.
MS_RDONLY = 1
MS_NOSUID = 2
MS_NODEV = 4
MS_NOEXEC = 8
MS_SYNCHRONOUS = 16
MS_REMOUNT = 32
MS_MANDLOCK = 64
MS_DIRSYNC = 128
MS_NOATIME = 1024
MS_NODIRATIME = 2048
MS_BIND = 4096
MS_MOVE = 8192
MS_REC = 16384
MS_SILENT = 32768
MS_POSIXACL = 1 << 16
MS_UNBINDABLE = 1 << 17
MS_PRIVATE = 1 << 18
MS_SLAVE = 1 << 19
MS_SHARED = 1 << 20
MS_RELATIME = 1 << 21
MS_KERNMOUNT = 1 << 22
MS_I_VERSION = 1 << 23
MS_STRICTATIME = 1 << 24
MS_ACTIVE = 1 << 30
MS_NOUSER = 1 << 31


def Mount(source, target, fstype, flags, data=''):
  """Call the mount(2) func; see the man page for details."""
  libc = ctypes.CDLL(ctypes.util.find_library('c'), use_errno=True)
  # These fields might be a string or 0 (for NULL).  Convert to bytes.
  def _MaybeEncode(s):
    return s.encode('utf-8') if isinstance(s, six.string_types) else s
  if libc.mount(_MaybeEncode(source), _MaybeEncode(target),
                _MaybeEncode(fstype), ctypes.c_int(flags),
                _MaybeEncode(data)) != 0:
    e = ctypes.get_errno()
    raise OSError(e, os.strerror(e))


def MountDir(src_path, dst_path, fs_type=None, sudo=True, makedirs=True,
             mount_opts=('nodev', 'noexec', 'nosuid'), skip_mtab=False,
             **kwargs):
  """Mount |src_path| at |dst_path|

  Args:
    src_path: Source of the new mount.
    dst_path: Where to mount things.
    fs_type: Specify the filesystem type to use.  Defaults to autodetect.
    sudo: Run through sudo.
    makedirs: Create |dst_path| if it doesn't exist.
    mount_opts: List of options to pass to `mount`.
    skip_mtab: Whether to write new entries to /etc/mtab.
    kwargs: Pass all other args to run.
  """
  if sudo:
    runcmd = cros_build_lib.sudo_run
  else:
    runcmd = cros_build_lib.run

  if makedirs:
    SafeMakedirs(dst_path, sudo=sudo)

  cmd = ['mount', src_path, dst_path]
  if skip_mtab:
    cmd += ['-n']
  if fs_type:
    cmd += ['-t', fs_type]
  if mount_opts:
    cmd += ['-o', ','.join(mount_opts)]
  runcmd(cmd, **kwargs)


def MountTmpfsDir(path, name='osutils.tmpfs', size='5G',
                  mount_opts=('nodev', 'noexec', 'nosuid'), **kwargs):
  """Mount a tmpfs at |path|

  Args:
    path: Directory to mount the tmpfs.
    name: Friendly name to include in mount output.
    size: Size of the temp fs.
    mount_opts: List of options to pass to `mount`.
    kwargs: Pass all other args to MountDir.
  """
  mount_opts = list(mount_opts) + ['size=%s' % size]
  MountDir(name, path, fs_type='tmpfs', mount_opts=mount_opts, **kwargs)


def UmountDir(path, lazy=True, sudo=True, cleanup=True):
  """Unmount a previously mounted temp fs mount.

  Args:
    path: Directory to unmount.
    lazy: Whether to do a lazy unmount.
    sudo: Run through sudo.
    cleanup: Whether to delete the |path| after unmounting.
             Note: Does not work when |lazy| is set.
  """
  if sudo:
    runcmd = cros_build_lib.sudo_run
  else:
    runcmd = cros_build_lib.run

  cmd = ['umount', '-d', path]
  if lazy:
    cmd += ['-l']
  runcmd(cmd, debug_level=logging.DEBUG)

  if cleanup:
    # We will randomly get EBUSY here even when the umount worked.  Suspect
    # this is due to the host distro doing stupid crap on us like autoscanning
    # directories when they get mounted.
    def _retry(e):
      # When we're using `rm` (which is required for sudo), we can't cleanly
      # detect the aforementioned failure.  This is because `rm` will see the
      # errno, handle itself, and then do exit(1).  Which means all we see is
      # that rm failed.  Assume it's this issue as -rf will ignore most things.
      if isinstance(e, cros_build_lib.RunCommandError):
        return True
      elif isinstance(e, OSError):
        # When we aren't using sudo, we do the unlink ourselves, so the exact
        # errno is bubbled up to us and we can detect it specifically without
        # potentially ignoring all other possible failures.
        return e.errno == errno.EBUSY
      else:
        # Something else, we don't know so do not retry.
        return False
    retry_util.GenericRetry(_retry, 60, RmDir, path, sudo=sudo, sleep=1)


def UmountTree(path):
  """Unmounts |path| and any submounts under it."""
  # Scrape it from /proc/mounts since it's easily accessible;
  # additionally, unmount in reverse order of what's listed there
  # rather than trying a reverse sorting; it's possible for
  # mount /z /foon
  # mount /foon/blah -o loop /a
  # which reverse sorting cannot handle.
  path = os.path.realpath(path).rstrip('/') + '/'
  mounts = [mtab.destination for mtab in IterateMountPoints() if
            mtab.destination.startswith(path) or
            mtab.destination == path.rstrip('/')]

  for mount_pt in reversed(mounts):
    UmountDir(mount_pt, lazy=False, cleanup=False)


def SetEnvironment(env):
  """Restore the environment variables to that of passed in dictionary."""
  os.environ.clear()
  os.environ.update(env)


def SourceEnvironment(script, whitelist, ifs=',', env=None, multiline=False):
  """Returns the environment exported by a shell script.

  Note that the script is actually executed (sourced), so do not use this on
  files that have side effects (such as modify the file system).  Stdout will
  be sent to /dev/null, so just echoing is OK.

  Args:
    script: The shell script to 'source'.
    whitelist: An iterable of environment variables to retrieve values for.
    ifs: When showing arrays, what separator to use.
    env: A dict of the initial env to pass down.  You can also pass it None
         (to clear the env) or True (to preserve the current env).
    multiline: Allow a variable to span multiple lines.

  Returns:
    A dictionary containing the values of the whitelisted environment
    variables that are set.
  """
  dump_script = ['source "%s" >/dev/null' % script,
                 'IFS="%s"' % ifs]
  for var in whitelist:
    # Note: If we want to get more exact results out of bash, we should switch
    # to using `declare -p "${var}"`.  It would require writing a custom parser
    # here, but it would be more robust.
    dump_script.append(
        '[[ "${%(var)s+set}" == "set" ]] && echo "%(var)s=\\"${%(var)s[*]}\\""'
        % {'var': var})
  dump_script.append('exit 0')

  if env is None:
    env = {}
  elif env is True:
    env = None
  output = cros_build_lib.run(['bash'], env=env, stdout=True,
                              stderr=True, print_cmd=False,
                              encoding='utf-8',
                              input='\n'.join(dump_script)).output
  return key_value_store.LoadData(output, multiline=multiline)


def ListBlockDevices(device_path=None, in_bytes=False):
  """Lists all block devices.

  Args:
    device_path: device path (e.g. /dev/sdc).
    in_bytes: whether to display size in bytes.

  Returns:
    A list of BlockDevice items with attributes 'NAME', 'RM', 'TYPE',
    'SIZE' (RM stands for removable).
  """
  keys = ['NAME', 'RM', 'TYPE', 'SIZE']
  BlockDevice = collections.namedtuple('BlockDevice', keys)

  cmd = ['lsblk', '--pairs']
  if in_bytes:
    cmd.append('--bytes')

  if device_path:
    cmd.append(device_path)

  cmd += ['--output', ','.join(keys)]
  result = cros_build_lib.dbg_run(cmd, capture_output=True, encoding='utf-8')
  devices = []
  for line in result.stdout.strip().splitlines():
    d = {}
    for k, v in re.findall(r'(\S+?)=\"(.+?)\"', line):
      d[k] = v

    devices.append(BlockDevice(**d))

  return devices


def GetDeviceInfo(device, keyword='model'):
  """Get information of |device| by searching through device path.

    Looks for the file named |keyword| in the path upwards from
    /sys/block/|device|/device. This path is a symlink and will be fully
    expanded when searching.

  Args:
    device: Device name (e.g. 'sdc').
    keyword: The filename to look for (e.g. product, model).

  Returns:
    The content of the |keyword| file.
  """
  device_path = os.path.join('/sys', 'block', device)
  if not os.path.isdir(device_path):
    raise ValueError('%s is not a valid device path.' % device_path)

  path_list = ExpandPath(os.path.join(device_path, 'device')).split(os.path.sep)
  while len(path_list) > 2:
    target = os.path.join(os.path.sep.join(path_list), keyword)
    if os.path.isfile(target):
      return ReadFile(target).strip()

    path_list = path_list[:-1]


def GetDeviceSize(device_path, in_bytes=False):
  """Returns the size of |device|.

  Args:
    device_path: Device path (e.g. '/dev/sdc').
    in_bytes: If set True, returns the size in bytes.

  Returns:
    Size of the device in human readable format unless |in_bytes| is set.
  """
  devices = ListBlockDevices(device_path=device_path, in_bytes=in_bytes)
  for d in devices:
    if d.TYPE == 'disk':
      return int(d.SIZE) if in_bytes else d.SIZE

  raise ValueError('No size info of %s is found.' % device_path)


FileInfo = collections.namedtuple(
    'FileInfo', ['path', 'owner', 'size', 'atime', 'mtime'])


def StatFilesInDirectory(path, recursive=False, to_string=False):
  """Stat files in the directory |path|.

  Args:
    path: Path to the target directory.
    recursive: Whether to recurisvely list all files in |path|.
    to_string: Whether to return a string containing the metadata of the
      files.

  Returns:
    If |to_string| is False, returns a list of FileInfo objects. Otherwise,
    returns a string of metadata of the files.
  """
  path = ExpandPath(path)
  def ToFileInfo(path, stat_val):
    return FileInfo(path,
                    pwd.getpwuid(stat_val.st_uid)[0],
                    stat_val.st_size,
                    datetime.datetime.fromtimestamp(stat_val.st_atime),
                    datetime.datetime.fromtimestamp(stat_val.st_mtime))

  file_infos = []
  for root, dirs, files in os.walk(path, topdown=True):
    for filename in dirs + files:
      filepath = os.path.join(root, filename)
      file_infos.append(ToFileInfo(filepath, os.lstat(filepath)))

    if not recursive:
      # Process only the top-most directory.
      break

  if not to_string:
    return file_infos

  msg = 'Listing the content of %s' % path
  msg_format = ('Path: {x.path}, Owner: {x.owner}, Size: {x.size} bytes, '
                'Accessed: {x.atime}, Modified: {x.mtime}')
  msg = '%s\n%s' % (msg,
                    '\n'.join([msg_format.format(x=x) for x in file_infos]))
  return msg


@contextlib.contextmanager
def ChdirContext(target_dir):
  """A context manager to chdir() into |target_dir| and back out on exit.

  Args:
    target_dir: A target directory to chdir into.
  """

  cwd = os.getcwd()
  os.chdir(target_dir)
  try:
    yield
  finally:
    os.chdir(cwd)


def _SameFileSystem(path1, path2):
  """Determine whether two paths are on the same filesystem.

  Be resilient to nonsense paths. Return False instead of blowing up.
  """
  try:
    return os.stat(path1).st_dev == os.stat(path2).st_dev
  except OSError:
    return False


class MountOverlayContext(object):
  """A context manager for mounting an OverlayFS directory.

  An overlay filesystem will be mounted at |mount_dir|, and will be unmounted
  when the context exits.
  """

  OVERLAY_FS_MOUNT_ERRORS = (32,)
  def __init__(self, lower_dir, upper_dir, mount_dir, cleanup=False):
    """Initialize.

    Args:
      lower_dir: The lower directory (read-only).
      upper_dir: The upper directory (read-write).
      mount_dir: The mount point for the merged overlay.
      cleanup: Whether to remove the mount point after unmounting. This uses an
          internal retry logic for cases where unmount is successful but the
          directory still appears busy, and is generally more resilient than
          removing it independently.
    """
    self._lower_dir = lower_dir
    self._upper_dir = upper_dir
    self._mount_dir = mount_dir
    self._cleanup = cleanup
    self.tempdir = None

  def __enter__(self):
    # Upstream Kernel 3.18 and the ubuntu backport of overlayfs have different
    # APIs. We must support both.
    try_legacy = False
    stashed_e_overlay_str = None

    # We must ensure that upperdir and workdir are on the same filesystem.
    if _SameFileSystem(self._upper_dir, GetGlobalTempDir()):
      _TempDirSetup(self)
    elif _SameFileSystem(self._upper_dir, os.path.dirname(self._upper_dir)):
      _TempDirSetup(self, base_dir=os.path.dirname(self._upper_dir))
    else:
      logging.debug('Could create find a workdir on the same filesystem as %s. '
                    'Trying legacy API instead.',
                    self._upper_dir)
      try_legacy = True

    if not try_legacy:
      try:
        MountDir('overlay', self._mount_dir, fs_type='overlay', makedirs=False,
                 mount_opts=('lowerdir=%s' % self._lower_dir,
                             'upperdir=%s' % self._upper_dir,
                             'workdir=%s' % self.tempdir),
                 quiet=True)
      except cros_build_lib.RunCommandError as e_overlay:
        if e_overlay.result.returncode not in self.OVERLAY_FS_MOUNT_ERRORS:
          raise
        logging.debug('Failed to mount overlay filesystem. Trying legacy API.')
        stashed_e_overlay_str = str(e_overlay)
        try_legacy = True

    if try_legacy:
      try:
        MountDir('overlayfs', self._mount_dir, fs_type='overlayfs',
                 makedirs=False,
                 mount_opts=('lowerdir=%s' % self._lower_dir,
                             'upperdir=%s' % self._upper_dir),
                 quiet=True)
      except cros_build_lib.RunCommandError as e_overlayfs:
        logging.error('All attempts at mounting overlay filesystem failed.')
        if stashed_e_overlay_str is not None:
          logging.error('overlay: %s', stashed_e_overlay_str)
        logging.error('overlayfs: %s', str(e_overlayfs))
        raise

    return self

  def __exit__(self, exc_type, exc_value, traceback):
    UmountDir(self._mount_dir, cleanup=self._cleanup)
    _TempDirTearDown(self, force_sudo=True)


MountInfo = collections.namedtuple(
    'MountInfo',
    'source destination filesystem options')


def IterateMountPoints(proc_file='/proc/mounts'):
  """Iterate over all mounts as reported by "/proc/mounts".

  Args:
    proc_file: A path to a file whose content is similar to /proc/mounts.
      Default to "/proc/mounts" itself.

  Returns:
    A generator that yields MountInfo objects.
  """
  with open(proc_file) as f:
    for line in f:
      # Escape any \xxx to a char.
      source, destination, filesystem, options, _, _ = [
          re.sub(r'\\([0-7]{3})', lambda m: chr(int(m.group(1), 8)), x)
          for x in line.split()
      ]
      mtab = MountInfo(source, destination, filesystem, options)
      yield mtab


def IsMounted(path):
  """Determine if |path| is already mounted or not."""
  path = os.path.realpath(path).rstrip('/')
  mounts = [mtab.destination for mtab in IterateMountPoints()]
  if path in mounts:
    return True

  return False


def ResolveSymlinkInRoot(file_name, root):
  """Resolve a symlink |file_name| relative to |root|.

  This can be used to resolve absolute symlinks within an alternative root
  path (i.e. chroot). For example:

    ROOT-A/absolute_symlink --> /an/abs/path
    ROOT-A/relative_symlink --> a/relative/path

    absolute_symlink will be resolved to ROOT-A/an/abs/path
    relative_symlink will be resolved to ROOT-A/a/relative/path

  Args:
    file_name (str): A path to the file.
    root (str|None): A path to the root directory.

  Returns:
    |file_name| if |file_name| is not a symlink. Otherwise, the ultimate path
    that |file_name| points to, with links resolved relative to |root|.
  """
  count = 0
  while os.path.islink(file_name):
    count += 1
    if count > 128:
      raise ValueError('Too many link levels for %s.' % file_name)
    link = os.readlink(file_name)
    if link.startswith('/'):
      file_name = os.path.join(root, link[1:]) if root else link
    else:
      file_name = os.path.join(os.path.dirname(file_name), link)
  return file_name


def ResolveSymlink(file_name):
  """Resolve a symlink |file_name| to an absolute path.

  This is similar to ResolveSymlinkInRoot, but does not resolve absolute
  symlinks to an alternative root, and normalizes the path before returning.

  Args:
    file_name (str): The symlink.

  Returns:
    str - |file_name| if |file_name| is not a symlink. Otherwise, the ultimate
    path that |file_name| points to.
  """
  return os.path.realpath(ResolveSymlinkInRoot(file_name, None))


def IsInsideVm():
  """Return True if we are running inside a virtual machine.

  The detection is based on the model of the hard drive.
  """
  for blk_model in glob.glob('/sys/block/*/device/model'):
    if os.path.isfile(blk_model):
      model = ReadFile(blk_model)
      if model.startswith('VBOX') or model.startswith('VMware'):
        return True

  return False
