# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common file and os related utilities, including tempdir manipulation."""

import contextlib
import errno
import logging
import os
import shutil
import cStringIO
import tempfile
from chromite.lib import cros_build_lib

# Env vars that tempdir can be gotten from; minimally, this
# needs to match python's tempfile module and match normal
# unix standards.
_TEMPDIR_ENV_VARS = ('TMPDIR', 'TEMP', 'TMP')


def ExpandPath(path):
  """Returns path after passing through realpath and expanduser."""
  return os.path.realpath(os.path.expanduser(path))


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

  with open(write_path, mode) as f:
    f.writelines(cros_build_lib.iflatten_instance(content))

  if not atomic:
    return

  try:
    os.rename(write_path, path)
  except EnvironmentError:
    SafeUnlink(write_path)
    raise


def Touch(path, makedirs=False):
  """Simulate unix touch. Create if doesn't exist and update its timestamp.

  Arguments:
    path: a string, file name of the file to touch (creating if not present).
    makedirs: a Boolean - if set to True - create the directories in the path.
              if they do not exist.
  """
  if makedirs:
    SafeMakedirs(os.path.dirname(path))

  # Create the file if nonexistant.
  open(path, 'a').close()
  # Update timestamp to right now.
  os.utime(path, None)


def ReadFile(path, mode='r'):
  """Read a given file on disk.  Primarily useful for one off small files."""
  with open(path, mode) as f:
    return f.read()


def SafeUnlink(path, sudo=False):
  """Unlink a file from disk, ignoring if it doesn't exist.

  Returns True if the file existed and was removed, False if it didn't exist.
  """
  if sudo:
    try:
      cros_build_lib.SudoRunCommand(
          ['rm', '--',  path], print_cmd=False, redirect_stderr=True)
      return True
    except cros_build_lib.RunCommandError:
      if os.path.exists(path):
        # Technically racey, but oh well; very hard to actually hit...
        raise
      return False
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


def RmDir(path, ignore_missing=False, sudo=False):
  """Recursively remove a directory.

  Arguments:
    ignore_missing: Do not error when path does not exist.
    sudo: Remove directories as root.
  """
  if sudo:
    try:
      cros_build_lib.SudoRunCommand(
          ['rm', '-r%s' % ('f' if ignore_missing else '',), '--', path],
          debug_level=logging.DEBUG,
          redirect_stdout=True, redirect_stderr=True)
    except cros_build_lib.RunCommandError, e:
      if not ignore_missing or os.path.exists(path):
        # If we're not ignoring the rm ENOENT equivalent, throw it;
        # if the pathway still exists, something failed, thus throw it.
        raise
  else:
    try:
      shutil.rmtree(path)
    except EnvironmentError, e:
      if not ignore_missing or e.errno != errno.ENOENT:
        raise


def Which(binary, path=None):
  """Return the absolute path to the specified binary.

  Arguments:
    binary: The binary to look for.
    path: Search path. Defaults to os.environ['PATH'].

  Returns the specified binary if found. Otherwise returns None.
  """
  if path is None:
    path = os.environ.get('PATH', '')
  for p in path.split(':'):
    p = os.path.join(p, binary)
    if os.access(p, os.X_OK):
      return p
  return None


def FindMissingBinaries(needed_tools):
  """Verifies that the required tools are present on the system.

  This is especially important for scripts that are intended to run
  outside the chroot.

  Arguments:
    needed_tools: an array of string specified binaries to look for.

  Returns:
    If all tools are found, returns the empty list. Otherwise, returns the
    list of missing tools.
  """
  return [binary for binary in needed_tools if Which(binary) is None]


def IteratePathParents(start_path):
  """Generator that iterates through a directory's parents.

  Yields:
    The passed-in path, along with its parents.  i.e.,
    IteratePathParents('/usr/local') would yield '/usr/local', '/usr/', and '/'.

  Arguments:
    start_path: The path to start from.
  """
  path = os.path.abspath(start_path)
  yield path
  while path.strip('/'):
    path = os.path.dirname(path)
    yield path


def FindInPathParents(path_to_find, start_path, test_func=None):
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

  Arguments:
    rel_path: The relative path to look for.
    start_path: The path to start the search from.  If |start_path| is a
      directory, it will be included in the directories that are searched.
    test_func: The function to use to verify the relative path.  Defaults to
      os.path.exists.  The function will be passed one argument - the target
      path to test.  A True return value will cause AscendingLookup to return
      the target.
  """
  if test_func is None:
    test_func = os.path.exists
  for path in IteratePathParents(start_path):
    target = os.path.join(path, path_to_find)
    if test_func(target):
      return target
  return None


# pylint: disable=W0212,R0904,W0702
def _TempDirSetup(self, prefix='tmp', update_env=True, base_dir=None):
  """Generate a tempdir, modifying the object, and env to use it.

  Specifically, if update_env is True, then from this invocation forward,
  python and all subprocesses will use this location for their tempdir.

  The matching _TempDirTearDown restores the env to what it was.
  """
  # Stash the old tempdir that was used so we can
  # switch it back on the way out.
  self.tempdir = tempfile.mkdtemp(prefix=prefix, dir=base_dir)
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
      # Finally, adjust python's cached value (we know it's cached by here
      # since we invoked _get_default_tempdir from above).  Note this
      # is necessary since we want *all* output from that point
      # forward to go to this location.
      tempfile.tempdir = self.tempdir


# pylint: disable=W0212,R0904,W0702
def _TempDirTearDown(self, force_sudo):
  # Note that _TempDirSetup may have failed, resulting in these attributes
  # not being set; this is why we use getattr here (and must).
  tempdir = getattr(self, 'tempdir', None)
  try:
    if tempdir is not None:
      RmDir(tempdir, ignore_missing=True, sudo=force_sudo)
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
def TempDirContextManager(prefix='tmp', base_dir=None, storage=None,
                          sudo_rm=False):
  """ContextManager constraining all tempfile/TMPDIR activity to a tempdir

  Arguments:
    prefix: See tempfile.mkdtemp documentation.
    base_dir: The directory to place the temporary directory.
    storage: The object that will have its 'tempdir' attribute set.
    sudo_rm: Whether the temporary dir will need root privileges to remove.
  """
  if storage is None:
    # Fake up a mutable object.
    # TOOD(build): rebase this to EasyAttr from cros_test_lib once it moves
    # to an importable location.
    class foon(object):
      pass
    storage = foon()

  _TempDirSetup(storage, base_dir=base_dir, prefix=prefix)
  try:
    yield storage.tempdir
  finally:
    _TempDirTearDown(storage, sudo_rm)


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


def SetEnvironment(env):
  """Restore the environment variables to that of passed in dictionary."""
  os.environ.clear()
  os.environ.update(env)


def SourceEnvironment(script, whitelist, ifs=',', env=None):
  """Returns the environment exported by a shell script.

  Note that the script is actually executed (sourced), so do not use this on
  files that have side effects (such as modify the file system).  Stdout will
  be sent to /dev/null, so just echoing is OK.

  Arguments:
    script: The shell script to 'source'.
    whitelist: An iterable of environment variables to retrieve values for.
    ifs: When showing arrays, what separator to use.
    env: A dict of the initial env to pass down.  You can also pass it None
         (to clear the env) or True (to preserve the current env).

  Returns:
    A dictionary containing the values of the whitelisted environment
    variables that are set.
  """
  dump_script = ['source "%s" >/dev/null' % script,
                 'IFS="%s"' % ifs]
  for var in whitelist:
    dump_script.append(
        '[[ "${%(var)s+set}" == "set" ]] && echo %(var)s="${%(var)s[*]}"'
        % {'var': var})
  dump_script.append('exit 0')

  if env is None:
    env = {}
  elif env is True:
    env = None
  output = cros_build_lib.RunCommand(['bash'], env=env, redirect_stdout=True,
                                     print_cmd=False,
                                     input='\n'.join(dump_script)).output
  return cros_build_lib.LoadKeyValueFile(cStringIO.StringIO(output))
