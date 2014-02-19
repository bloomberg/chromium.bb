# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common file and os related utilities, including tempdir manipulation."""

import collections
import errno
import logging
import os
import pwd
import re
import shutil
import signal
import cStringIO
import tempfile
from chromite.lib import cros_build_lib
from chromite.lib import retry_util

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


def ExpandPath(path):
  """Returns path after passing through realpath and expanduser."""
  return os.path.realpath(os.path.expanduser(path))


def WriteFile(path, content, mode='w', atomic=False, makedirs=False):
  """Write the given content to disk.

  Args:
    path: Pathway to write the content to.
    content: Content to write.  May be either an iterable, or a string.
    mode: Optional; if binary mode is necessary, pass 'wb'.  If appending is
          desired, 'w+', etc.
    atomic: If the updating of the file should be done atomically.  Note this
            option is incompatible w/ append mode.
    makedirs: If True, create missing leading directories in the path.
  """
  write_path = path
  if atomic:
    write_path = path + '.tmp'

  if makedirs:
    SafeMakedirs(os.path.dirname(path))

  with open(write_path, mode) as f:
    f.writelines(cros_build_lib.iflatten_instance(content))

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


def ReadFile(path, mode='r'):
  """Read a given file on disk.  Primarily useful for one off small files."""
  with open(path, mode) as f:
    return f.read()


def SafeUnlink(path, sudo=False):
  """Unlink a file from disk, ignoring if it doesn't exist.

  Returns:
    True if the file existed and was removed, False if it didn't exist.
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
  except EnvironmentError as e:
    if e.errno != errno.ENOENT:
      raise
  return False


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
    EnvironmentError: if the makedir failed and it was non sudo.
    RunCommandError: If sudo mode, and the command failed for any reason.
  """
  if sudo:
    if os.path.isdir(path):
      return False
    cros_build_lib.SudoRunCommand(
        ['mkdir', '-p', '--mode', oct(mode), path], user=user, print_cmd=False,
        redirect_stderr=True, redirect_stdout=True)
    return True

  try:
    os.makedirs(path, mode)
    return True
  except EnvironmentError as e:
    if e.errno != errno.EEXIST or not os.path.isdir(path):
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
    raise MakingDirsAsRoot('Refusing to create %s as root!' % path)

  created = SafeMakedirs(path, mode=mode, user=user)
  # Temporary fix: if the directory already exists and is owned by
  # root, chown it. This corrects existing root-owned directories.
  if not created:
    stat_info = os.stat(path)
    if stat_info.st_uid == 0:
      cros_build_lib.SudoRunCommand(['chown', user, path],
                                    print_cmd=False,
                                    redirect_stderr=True,
                                    redirect_stdout=True)
  return created


def RmDir(path, ignore_missing=False, sudo=False):
  """Recursively remove a directory.

  Args:
    path: Path of directory to remove.
    ignore_missing: Do not error when path does not exist.
    sudo: Remove directories as root.
  """
  if sudo:
    try:
      cros_build_lib.SudoRunCommand(
          ['rm', '-r%s' % ('f' if ignore_missing else '',), '--', path],
          debug_level=logging.DEBUG,
          redirect_stdout=True, redirect_stderr=True)
    except cros_build_lib.RunCommandError as e:
      if not ignore_missing or os.path.exists(path):
        # If we're not ignoring the rm ENOENT equivalent, throw it;
        # if the pathway still exists, something failed, thus throw it.
        raise
  else:
    try:
      shutil.rmtree(path)
    except EnvironmentError as e:
      if not ignore_missing or e.errno != errno.ENOENT:
        raise


def Which(binary, path=None, mode=os.X_OK):
  """Return the absolute path to the specified binary.

  Args:
    binary: The binary to look for.
    path: Search path. Defaults to os.environ['PATH'].
    mode: File mode to check on the binary.

  Returns:
    The full path to |binary| if found (with the right mode). Otherwise, None.
  """
  if path is None:
    path = os.environ.get('PATH', '')
  for p in path.split(os.pathsep):
    p = os.path.join(p, binary)
    if os.path.isfile(p) and os.access(p, mode):
      return p
  return None


def FindDepotTools():
  """Returns the location of depot_tools if it is in $PATH."""
  gclient_dir = os.path.dirname(Which('gclient.py', mode=os.F_OK) or '')
  gitcl_dir = os.path.dirname(Which('git_cl.py', mode=os.F_OK) or '')
  if gclient_dir and gclient_dir == gitcl_dir:
    return gclient_dir


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


def IteratePathParents(start_path):
  """Generator that iterates through a directory's parents.

  Args:
    start_path: The path to start from.

  Yields:
    The passed-in path, along with its parents.  i.e.,
    IteratePathParents('/usr/local') would yield '/usr/local', '/usr/', and '/'.
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

  Args:
    path_to_find: The relative path to look for.
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
  with tempfile._once_lock:
    old_tempdir_value = tempfile._get_default_tempdir()
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


def _TempDirTearDown(self, force_sudo):
  # Note that _TempDirSetup may have failed, resulting in these attributes
  # not being set; this is why we use getattr here (and must).
  tempdir = getattr(self, 'tempdir', None)
  try:
    if tempdir is not None:
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
      storage: The object that will have its 'tempdir' attribute set.
      sudo_rm: Whether the temporary dir will need root privileges to remove.
    """
    self.kwargs = kwargs.copy()
    self.sudo_rm = kwargs.pop('sudo_rm', False)
    self.tempdir = None
    _TempDirSetup(self, **kwargs)

  def Cleanup(self):
    """Clean up the temporary directory."""
    if self.tempdir is not None:
      _TempDirTearDown(self, self.sudo_rm)
      self.tempdir = None

  def __enter__(self):
    """Return the temporary directory."""
    return self.tempdir

  def __exit__(self, _type, _value, _traceback):
    self.Cleanup()

  def __del__(self):
    self.Cleanup()


# pylint: disable=W0212,R0904,W0702
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


def MountDir(src_path, dst_path, fs_type=None, sudo=True, makedirs=True,
             mount_opts=('nodev', 'noexec', 'nosuid'), **kwargs):
  """Mount |src_path| at |dst_path|

  Args:
    src_path: Directory to mount the tmpfs.
    dst_path: Directory to mount the tmpfs.
    fs_type: Specify the filesystem type to use.  Defaults to autodetect.
    sudo: Run through sudo.
    makedirs: Create |dst_path| if it doesn't exist.
    mount_opts: List of options to pass to `mount`.
    kwargs: Pass all other args to RunCommand.
  """
  if sudo:
    runcmd = cros_build_lib.SudoRunCommand
  else:
    runcmd = cros_build_lib.RunCommand

  if makedirs:
    SafeMakedirs(dst_path, sudo=sudo)

  cmd = ['mount', src_path, dst_path]
  if fs_type:
    cmd += ['-t', fs_type]
  runcmd(cmd + ['-o', ','.join(mount_opts)], **kwargs)


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
    runcmd = cros_build_lib.SudoRunCommand
  else:
    runcmd = cros_build_lib.RunCommand

  cmd = ['umount', '-d', path]
  if lazy:
    cmd += ['-l']
  runcmd(cmd)

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
      else:
        # When we aren't using sudo, we do the unlink ourselves, so the exact
        # errno is bubbled up to us and we can detect it specifically without
        # potentially ignoring all other possible failures.
        return e.errno == errno.EBUSY
    retry_util.GenericRetry(_retry, 5, RmDir, path, sudo=sudo, sleep=1)


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
    dump_script.append(
        '[[ "${%(var)s+set}" == "set" ]] && echo %(var)s="${%(var)s[*]}"'
        % {'var': var})
  dump_script.append('exit 0')

  if env is None:
    env = {}
  elif env is True:
    env = None
  output = cros_build_lib.RunCommand(['bash'], env=env, redirect_stdout=True,
                                     redirect_stderr=True, print_cmd=False,
                                     input='\n'.join(dump_script)).output
  return cros_build_lib.LoadKeyValueFile(cStringIO.StringIO(output),
                                         multiline=multiline)


def StrSignal(sig_num):
  """Convert a signal number to the symbolic name

  Note: Some signal number have multiple names, so you might get
  back a confusing result like "SIGIOT|SIGABRT".  Since they have
  the same signal number, it's impossible to say which one is right.

  Args:
    sig_num: The numeric signal you wish to convert

  Returns:
    A string of the signal name(s)
  """
  sig_names = []
  for name, num in signal.__dict__.iteritems():
    if name.startswith('SIG') and num == sig_num:
      sig_names.append(name)
  if sig_names:
    return '|'.join(sig_names)
  else:
    return 'SIG_%i' % sig_num


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
  output = cros_build_lib.RunCommand(cmd, capture_output=True).output.strip()
  devices = []
  for line in output.splitlines():
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
