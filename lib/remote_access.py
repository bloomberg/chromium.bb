#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library containing functions to access a remote test device."""

import glob
import logging
import os
import shutil
import socket
import stat
import tempfile
import time

from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import timeout_util


_path = os.path.dirname(os.path.realpath(__file__))
TEST_PRIVATE_KEY = os.path.normpath(
    os.path.join(_path, '../ssh_keys/testing_rsa'))
del _path

LOCALHOST = 'localhost'
LOCALHOST_IP = '127.0.0.1'
ROOT_ACCOUNT = 'root'

REBOOT_MARKER = '/tmp/awaiting_reboot'
REBOOT_MAX_WAIT = 120
REBOOT_SSH_CONNECT_TIMEOUT = 2
REBOOT_SSH_CONNECT_ATTEMPTS = 2
CHECK_INTERVAL = 5
DEFAULT_SSH_PORT = 22
SSH_ERROR_CODE = 255

# Dev/test packages are installed in these paths.
DEV_BIN_PATHS = '/usr/local/bin:/usr/local/sbin'


def NormalizePort(port, str_ok=True):
  """Checks if |port| is a valid port number and returns the number.

  Args:
    port: The port to normalize.
    str_ok: Accept |port| in string. If set False, only accepts
      an integer. Defaults to True.

  Returns:
    A port number (integer).
  """
  err_msg = '%s is not a valid port number.' % port

  if not str_ok and not isinstance(port, int):
    raise ValueError(err_msg)

  port = int(port)
  if port <= 0 or port >= 65536:
    raise ValueError(err_msg)

  return port


def GetUnusedPort(ip=LOCALHOST, family=socket.AF_INET,
                  stype=socket.SOCK_STREAM):
  """Returns a currently unused port.

  Args:
    ip: IP to use to bind the port.
    family: Address family.
    stype: Socket type.

  Returns:
    A port number (integer).
  """
  s = None
  try:
    s = socket.socket(family, stype)
    s.bind((ip, 0))
    return s.getsockname()[1]
  except (socket.error, OSError):
    if s:
      s.close()


def RunCommandFuncWrapper(func, msg, *args, **kwargs):
  """Wraps a function that invokes cros_build_lib.RunCommand.

  If the command failed, logs warning |msg| if error_code_ok is set;
  logs error |msg| if error_code_ok is not set.

  Args:
    func: The function to call.
    msg: The message to display if the command failed.
    *args: Arguments to pass to |func|.
    **kwargs: Keyword arguments to pass to |func|.

  Returns:
    The result of |func|.

  Raises:
    cros_build_lib.RunCommandError if the command failed and error_code_ok
    is not set.
  """
  error_code_ok = kwargs.pop('error_code_ok', False)
  result = func(*args, error_code_ok=True, **kwargs)
  if result.returncode != 0 and not error_code_ok:
    raise cros_build_lib.RunCommandError(msg, result)

  if result.returncode != 0:
    logging.warning(msg)


def CompileSSHConnectSettings(ConnectTimeout=30, ConnectionAttempts=4):
  return ['-o', 'ConnectTimeout=%s' % ConnectTimeout,
          '-o', 'ConnectionAttempts=%s' % ConnectionAttempts,
          '-o', 'NumberOfPasswordPrompts=0',
          '-o', 'Protocol=2',
          '-o', 'ServerAliveInterval=10',
          '-o', 'ServerAliveCountMax=3',
          '-o', 'StrictHostKeyChecking=no',
          '-o', 'UserKnownHostsFile=/dev/null', ]


class SSHConnectionError(Exception):
  """Raised when SSH connection has failed."""


class RemoteAccess(object):
  """Provides access to a remote test machine."""

  DEFAULT_USERNAME = ROOT_ACCOUNT

  def __init__(self, remote_host, tempdir, port=None, username=None,
               private_key=None, debug_level=logging.DEBUG, interactive=True):
    """Construct the object.

    Args:
      remote_host: The ip or hostname of the remote test machine.  The test
                   machine should be running a ChromeOS test image.
      tempdir: A directory that RemoteAccess can use to store temporary files.
               It's the responsibility of the caller to remove it.
      port: The ssh port of the test machine to connect to.
      username: The ssh login username (default: root).
      private_key: The identify file to pass to `ssh -i` (default: testing_rsa).
      debug_level: Logging level to use for all RunCommand invocations.
      interactive: If set to False, pass /dev/null into stdin for the sh cmd.
    """
    self.tempdir = tempdir
    self.remote_host = remote_host
    self.port = port if port else DEFAULT_SSH_PORT
    self.username = username if username else self.DEFAULT_USERNAME
    self.debug_level = debug_level
    private_key_src = private_key if private_key else TEST_PRIVATE_KEY
    self.private_key = os.path.join(
        tempdir, os.path.basename(private_key_src))

    self.interactive = interactive
    shutil.copyfile(private_key_src, self.private_key)
    os.chmod(self.private_key, stat.S_IRUSR)

  @property
  def target_ssh_url(self):
    return '%s@%s' % (self.username, self.remote_host)

  def _GetSSHCmd(self, connect_settings=None):
    if connect_settings is None:
      connect_settings = CompileSSHConnectSettings()

    cmd = (['ssh', '-p', str(self.port)] +
           connect_settings +
           ['-i', self.private_key])
    if not self.interactive:
      cmd.append('-n')

    return cmd

  def RemoteSh(self, cmd, connect_settings=None, error_code_ok=False,
               remote_sudo=False, ssh_error_ok=False, **kwargs):
    """Run a sh command on the remote device through ssh.

    Args:
      cmd: The command string or list to run.
      connect_settings: The SSH connect settings to use.
      error_code_ok: Does not throw an exception when the command exits with a
                     non-zero returncode.  This does not cover the case where
                     the ssh command itself fails (return code 255).
                     See ssh_error_ok.
      ssh_error_ok: Does not throw an exception when the ssh command itself
                    fails (return code 255).
      remote_sudo: If set, run the command in remote shell with sudo.
      **kwargs: See cros_build_lib.RunCommand documentation.

    Returns:
      A CommandResult object.  The returncode is the returncode of the command,
      or 255 if ssh encountered an error (could not connect, connection
      interrupted, etc.)

    Raises:
      RunCommandError when error is not ignored through the error_code_ok flag.
      SSHConnectionError when ssh command error is not ignored through
      the ssh_error_ok flag.

    """
    kwargs.setdefault('capture_output', True)
    kwargs.setdefault('debug_level', self.debug_level)

    ssh_cmd = self._GetSSHCmd(connect_settings)
    ssh_cmd += [self.target_ssh_url, '--']

    if remote_sudo and self.username != ROOT_ACCOUNT:
      # Prepend sudo to cmd.
      ssh_cmd.append('sudo')

    if isinstance(cmd, basestring):
      ssh_cmd += [cmd]
    else:
      ssh_cmd += cmd

    try:
      return cros_build_lib.RunCommand(ssh_cmd, **kwargs)
    except cros_build_lib.RunCommandError as e:
      if ((e.result.returncode == SSH_ERROR_CODE and ssh_error_ok) or
          (e.result.returncode and e.result.returncode != SSH_ERROR_CODE
           and error_code_ok)):
        return e.result
      elif e.result.returncode == SSH_ERROR_CODE:
        raise SSHConnectionError(e.result.error)
      else:
        raise

  def _CheckIfRebooted(self):
    """Checks whether a remote device has rebooted successfully.

    This uses a rapidly-retried SSH connection, which will wait for at most
    about ten seconds. If the network returns an error (e.g. host unreachable)
    the actual delay may be shorter.

    Returns:
      Whether the device has successfully rebooted.
    """
    # In tests SSH seems to be waiting rather longer than would be expected
    # from these parameters. These values produce a ~5 second wait.
    connect_settings = CompileSSHConnectSettings(
        ConnectTimeout=REBOOT_SSH_CONNECT_TIMEOUT,
        ConnectionAttempts=REBOOT_SSH_CONNECT_ATTEMPTS)
    cmd = "[ ! -e '%s' ]" % REBOOT_MARKER
    result = self.RemoteSh(cmd, connect_settings=connect_settings,
                           error_code_ok=True, ssh_error_ok=True,
                           capture_output=True)

    errors = {0: 'Reboot complete.',
              1: 'Device has not yet shutdown.',
              255: 'Cannot connect to device; reboot in progress.'}
    if result.returncode not in errors:
      raise Exception('Unknown error code %s returned by %s.'
                      % (result.returncode, cmd))

    logging.info(errors[result.returncode])
    return result.returncode == 0

  def RemoteReboot(self):
    """Reboot the remote device."""
    logging.info('Rebooting %s...', self.remote_host)
    if self.username != ROOT_ACCOUNT:
      self.RemoteSh('sudo sh -c "touch %s && sudo reboot"' % REBOOT_MARKER)
    else:
      self.RemoteSh('touch %s && reboot' % REBOOT_MARKER)

    time.sleep(CHECK_INTERVAL)
    try:
      timeout_util.WaitForReturnTrue(self._CheckIfRebooted, REBOOT_MAX_WAIT,
                                     period=CHECK_INTERVAL)
    except timeout_util.TimeoutError:
      cros_build_lib.Die('Reboot has not completed after %s seconds; giving up.'
                         % (REBOOT_MAX_WAIT,))

  def Rsync(self, src, dest, to_local=False, follow_symlinks=False,
            recursive=True, inplace=False, verbose=False, sudo=False,
            remote_sudo=False, **kwargs):
    """Rsync a path to the remote device.

    Rsync a path to the remote device. If |to_local| is set True, it
    rsyncs the path from the remote device to the local machine.

    Args:
      src: The local src directory.
      dest: The remote dest directory.
      to_local: If set, rsync remote path to local path.
      follow_symlinks: If set, transform symlinks into referent
        path. Otherwise, copy symlinks as symlinks.
      recursive: Whether to recursively copy entire directories.
      inplace: If set, cause rsync to overwrite the dest files in place.  This
        conserves space, but has some side effects - see rsync man page.
      verbose: If set, print more verbose output during rsync file transfer.
      sudo: If set, invoke the command via sudo.
      remote_sudo: If set, run the command in remote shell with sudo.
      **kwargs: See cros_build_lib.RunCommand documentation.
    """
    kwargs.setdefault('debug_level', self.debug_level)

    ssh_cmd = ' '.join(self._GetSSHCmd())
    rsync_cmd = ['rsync', '--perms', '--verbose', '--times', '--compress',
                 '--omit-dir-times', '--exclude', '.svn']
    rsync_cmd.append('--copy-links' if follow_symlinks else '--links')
    rsync_sudo = 'sudo' if (
        remote_sudo and self.username != ROOT_ACCOUNT) else ''
    rsync_cmd += ['--rsync-path',
                  'PATH=%s:$PATH %s rsync' % (DEV_BIN_PATHS, rsync_sudo)]

    if verbose:
      rsync_cmd.append('--progress')
    if recursive:
      rsync_cmd.append('--recursive')
    if inplace:
      rsync_cmd.append('--inplace')

    if to_local:
      rsync_cmd += ['--rsh', ssh_cmd,
                    '[%s]:%s' % (self.target_ssh_url, src), dest]
    else:
      rsync_cmd += ['--rsh', ssh_cmd, src,
                    '[%s]:%s' % (self.target_ssh_url, dest)]

    rc_func = cros_build_lib.RunCommand
    if sudo:
      rc_func = cros_build_lib.SudoRunCommand
    return rc_func(rsync_cmd, print_cmd=verbose, **kwargs)

  def RsyncToLocal(self, *args, **kwargs):
    """Rsync a path from the remote device to the local machine."""
    return self.Rsync(*args, to_local=kwargs.pop('to_local', True), **kwargs)

  def Scp(self, src, dest, to_local=False, recursive=True, verbose=False,
          sudo=False, **kwargs):
    """Scp a file or directory to the remote device.

    Args:
      src: The local src file or directory.
      dest: The remote dest location.
      to_local: If set, scp remote path to local path.
      recursive: Whether to recursively copy entire directories.
      verbose: If set, print more verbose output during scp file transfer.
      sudo: If set, invoke the command via sudo.
      remote_sudo: If set, run the command in remote shell with sudo.
      **kwargs: See cros_build_lib.RunCommand documentation.

    Returns:
      A CommandResult object containing the information and return code of
      the scp command.
    """
    remote_sudo = kwargs.pop('remote_sudo', False)
    if remote_sudo and self.username != ROOT_ACCOUNT:
      # TODO: Implement scp with remote sudo.
      raise NotImplementedError('Cannot run scp with sudo!')

    kwargs.setdefault('debug_level', self.debug_level)
    # scp relies on 'scp' being in the $PATH of the non-interactive,
    # SSH login shell.
    scp_cmd = (['scp', '-P', str(self.port)] +
               CompileSSHConnectSettings(ConnectTimeout=60) +
               ['-i', self.private_key])

    if not self.interactive:
      scp_cmd.append('-n')

    if recursive:
      scp_cmd.append('-r')
    if verbose:
      scp_cmd.append('-v')

    if to_local:
      scp_cmd += ['%s:%s' % (self.target_ssh_url, src), dest]
    else:
      scp_cmd += glob.glob(src) + ['%s:%s' % (self.target_ssh_url, dest)]

    rc_func = cros_build_lib.RunCommand
    if sudo:
      rc_func = cros_build_lib.SudoRunCommand

    return rc_func(scp_cmd, print_cmd=verbose, **kwargs)

  def ScpToLocal(self, *args, **kwargs):
    """Scp a path from the remote device to the local machine."""
    return self.Scp(*args, to_local=kwargs.pop('to_local', True), **kwargs)

  def PipeToRemoteSh(self, producer_cmd, cmd, **kwargs):
    """Run a local command and pipe it to a remote sh command over ssh.

    Args:
      producer_cmd: Command to run locally with its results piped to |cmd|.
      cmd: Command to run on the remote device.
      **kwargs: See RemoteSh for documentation.
    """
    result = cros_build_lib.RunCommand(producer_cmd, stdout_to_pipe=True,
                                       print_cmd=False, capture_output=True)
    return self.RemoteSh(cmd, input=kwargs.pop('input', result.output),
                         **kwargs)


class RemoteDeviceHandler(object):
  """A wrapper of RemoteDevice."""

  def __init__(self, *args, **kwargs):
    """Creates a RemoteDevice object."""
    self.device = RemoteDevice(*args, **kwargs)

  def __enter__(self):
    """Return the temporary directory."""
    return self.device

  def __exit__(self, _type, _value, _traceback):
    """Cleans up the device."""
    self.device.Cleanup()


class ChromiumOSDeviceHandler(object):
  """A wrapper of ChromiumOSDevice."""

  def __init__(self, *args, **kwargs):
    """Creates a RemoteDevice object."""
    self.device = ChromiumOSDevice(*args, **kwargs)

  def __enter__(self):
    """Return the temporary directory."""
    return self.device

  def __exit__(self, _type, _value, _traceback):
    """Cleans up the device."""
    self.device.Cleanup()


class RemoteDevice(object):
  """Handling basic SSH communication with a remote device."""

  DEFAULT_BASE_DIR = '/tmp/remote-access'

  def __init__(self, hostname, port=None, username=None,
               base_dir=DEFAULT_BASE_DIR, connect_settings=None,
               private_key=None, debug_level=logging.DEBUG):
    """Initializes a RemoteDevice object.

    Args:
      hostname: The hostname of the device.
      port: The ssh port of the device.
      username: The ssh login username.
      base_dir: The base directory of the working directory on the device.
      connect_settings: Default SSH connection settings.
      private_key: The identify file to pass to `ssh -i`.
      debug_level: Setting debug level for logging.
    """
    self.hostname = hostname
    self.port = port
    self.username = username
    # The tempdir is for storing the rsa key and/or some temp files.
    self.tempdir = osutils.TempDir(prefix='ssh-tmp')
    self.connect_settings = (connect_settings if connect_settings else
                             CompileSSHConnectSettings())
    self.private_key = private_key
    self.agent = self._SetupSSH()
    self.debug_level = debug_level
    # Setup a working directory on the device.
    self.base_dir = base_dir

    # Do not call RunCommand here because we have not set up work directory yet.
    self.BaseRunCommand(['mkdir', '-p', self.base_dir])
    self.work_dir = self.BaseRunCommand(
        ['mktemp', '-d', '--tmpdir=%s' % base_dir],
        capture_output=True).output.strip()
    logging.debug(
        'The tempory working directory on the device is %s', self.work_dir)

    self.cleanup_cmds = []
    self.RegisterCleanupCmd(['rm', '-rf', self.work_dir])

  def _SetupSSH(self):
    """Setup the ssh connection with device."""
    return RemoteAccess(self.hostname, self.tempdir.tempdir, port=self.port,
                        username=self.username, private_key=self.private_key)

  def _HasRsync(self):
    """Checks if rsync exists on the device."""
    result = self.agent.RemoteSh(['PATH=%s:$PATH rsync' % DEV_BIN_PATHS,
                                  '--version'], error_code_ok=True)
    return result.returncode == 0

  def RegisterCleanupCmd(self, cmd, **kwargs):
    """Register a cleanup command to be run on the device in Cleanup().

    Args:
      cmd: command to run. See RemoteAccess.RemoteSh documentation.
      **kwargs: keyword arguments to pass along with cmd. See
        RemoteAccess.RemoteSh documentation.
    """
    self.cleanup_cmds.append((cmd, kwargs))

  def Cleanup(self):
    """Remove work/temp directories and run all registered cleanup commands."""
    for cmd, kwargs in self.cleanup_cmds:
      # We want to run through all cleanup commands even if there are errors.
      kwargs.setdefault('error_code_ok', True)
      self.BaseRunCommand(cmd, **kwargs)

    self.tempdir.Cleanup()

  def CopyToDevice(self, src, dest, mode=None, **kwargs):
    """Copy path to device."""
    msg = 'Could not copy %s to device.' % src
    if mode is None:
      # Use rsync by default if it exists.
      mode = 'rsync' if self._HasRsync() else 'scp'

    if mode == 'scp':
      # scp always follow symlinks
      kwargs.pop('follow_symlinks', None)
      func  = self.agent.Scp
    else:
      func = self.agent.Rsync

    return RunCommandFuncWrapper(func, msg, src, dest, **kwargs)

  def CopyFromDevice(self, src, dest, mode=None, **kwargs):
    """Copy path from device."""
    msg = 'Could not copy %s from device.' % src
    if mode is None:
      # Use rsync by default if it exists.
      mode = 'rsync' if self._HasRsync() else 'scp'

    if mode == 'scp':
      # scp always follow symlinks
      kwargs.pop('follow_symlinks', None)
      func  = self.agent.ScpToLocal
    else:
      func = self.agent.RsyncToLocal

    return RunCommandFuncWrapper(func, msg, src, dest, **kwargs)

  def CopyFromWorkDir(self, src, dest, **kwargs):
    """Copy path from working directory on the device."""
    return self.CopyFromDevice(os.path.join(self.work_dir, src), dest, **kwargs)

  def CopyToWorkDir(self, src, dest='', **kwargs):
    """Copy path to working directory on the device."""
    return self.CopyToDevice(src, os.path.join(self.work_dir, dest), **kwargs)

  def PipeOverSSH(self, filepath, cmd, **kwargs):
    """Cat a file and pipe over SSH."""
    producer_cmd = ['cat', filepath]
    return self.agent.PipeToRemoteSh(producer_cmd, cmd, **kwargs)

  def Reboot(self):
    """Reboot the device."""
    return self.agent.RemoteReboot()

  def BaseRunCommand(self, cmd, **kwargs):
    """Executes a shell command on the device with output captured by default.

    Args:
      cmd: command to run. See RemoteAccess.RemoteSh documentation.
      **kwargs: keyword arguments to pass along with cmd. See
        RemoteAccess.RemoteSh documentation.
    """
    kwargs.setdefault('debug_level', self.debug_level)
    kwargs.setdefault('connect_settings', self.connect_settings)
    try:
      return self.agent.RemoteSh(cmd, **kwargs)
    except SSHConnectionError:
      logging.error('Error connecting to device %s', self.hostname)
      raise

  def RunCommand(self, cmd, **kwargs):
    """Executes a shell command on the device with output captured by default.

    Also sets environment variables using dictionary provided by
    keyword argument |extra_env|.

    Args:
      cmd: command to run. See RemoteAccess.RemoteSh documentation.
      **kwargs: keyword arguments to pass along with cmd. See
        RemoteAccess.RemoteSh documentation.
    """
    new_cmd = cmd
    # Handle setting environment variables on the device by copying
    # and sourcing a temporary environment file.
    extra_env = kwargs.pop('extra_env', None)
    if extra_env:
      env_list = ['export %s=%s' % (k, cros_build_lib.ShellQuote(v))
                  for k, v in extra_env.iteritems()]
      remote_sudo = kwargs.pop('remote_sudo', False)
      with tempfile.NamedTemporaryFile(dir=self.tempdir.tempdir,
                                       prefix='env') as f:
        logging.debug('Environment variables: %s', ' '.join(env_list))
        osutils.WriteFile(f.name, '\n'.join(env_list))
        self.CopyToWorkDir(f.name)
        env_file = os.path.join(self.work_dir, os.path.basename(f.name))
        new_cmd = ['.', '%s;' % env_file]
        if remote_sudo and self.agent.username != ROOT_ACCOUNT:
          new_cmd += ['sudo', '-E']

        new_cmd += cmd

    return self.BaseRunCommand(new_cmd, **kwargs)


class ChromiumOSDevice(RemoteDevice):
  """Basic commands to interact with a ChromiumOS device over SSH connection."""

  MAKE_DEV_SSD_BIN = '/usr/share/vboot/bin/make_dev_ssd.sh'
  MOUNT_ROOTFS_RW_CMD = ['mount', '-o', 'remount,rw', '/']
  LIST_MOUNTS_CMD = ['cat', '/proc/mounts']
  GET_BOARD_CMD = ['grep', 'CHROMEOS_RELEASE_BOARD', '/etc/lsb-release']

  def __init__(self, *args, **kwargs):
    super(ChromiumOSDevice, self).__init__(*args, **kwargs)
    self.board = self._LearnBoard()
    self.path = self._GetPath()

  def _GetPath(self):
    """Gets $PATH on the device and prepend it with DEV_BIN_PATHS."""
    try:
      result = self.BaseRunCommand(['echo', "${PATH}"])
    except cros_build_lib.RunCommandError:
      logging.warning('Error detecting $PATH on the device.')
      raise

    return '%s:%s' % (DEV_BIN_PATHS, result.output.strip())

  def _RemountRootfsAsWritable(self):
    """Attempts to Remount the root partition."""
    logging.info("Remounting '/' with rw...")
    self.RunCommand(self.MOUNT_ROOTFS_RW_CMD, error_code_ok=True,
                    remote_sudo=True)

  def _RootfsIsReadOnly(self):
    """Returns True if rootfs on is mounted as read-only."""
    r = self.RunCommand(self.LIST_MOUNTS_CMD, capture_output=True)
    for line in r.output.splitlines():
      if not line:
        continue

      chunks = line.split()
      if chunks[1] == '/' and 'ro' in chunks[3].split(','):
        return True

    return False

  def _DisableRootfsVerification(self):
    """Disables device rootfs verification."""
    logging.info('Disabling rootfs verification on device...')
    self.RunCommand(
        [self.MAKE_DEV_SSD_BIN, '--remove_rootfs_verification', '--force'],
        error_code_ok=True, remote_sudo=True)
    # TODO(yjhong): Make sure an update is not pending.
    logging.info('Need to reboot to actually disable the verification.')
    self.Reboot()

  def MountRootfsReadWrite(self):
    """Checks mount types and remounts them as read-write if needed.

    Returns:
      True if rootfs is mounted as read-write. False otherwise.
    """
    if not self._RootfsIsReadOnly():
      return True

    # If the image on the device is built with rootfs verification
    # disabled, we can simply remount '/' as read-write.
    self._RemountRootfsAsWritable()

    if not self._RootfsIsReadOnly():
      return True

    logging.info('Unable to remount rootfs as rw (normal w/verified rootfs).')
    # If the image is built with rootfs verification, turn off the
    # rootfs verification. After reboot, the rootfs will be mounted as
    # read-write (there is no need to remount).
    self._DisableRootfsVerification()

    return not self._RootfsIsReadOnly()

  def _LearnBoard(self):
    """Grab the board reported by the remote device.

    In the case of multiple matches, uses the first one.  In the case of no
    entry or if the command failed, returns an empty string.
    """
    try:
      result = self.BaseRunCommand(self.GET_BOARD_CMD, capture_output=True)
    except cros_build_lib.RunCommandError:
      logging.warning('Error detecting the board.')
      return ''

    # In the case of multiple matches, use the first one.
    output = result.output.splitlines()
    if len(output) > 1:
      logging.debug('More than one board entry found!  Using the first one.')

    return output[0].strip().partition('=')[-1]

  def RunCommand(self, cmd, **kwargs):
    """Executes a shell command on the device with output captured by default.

    Also makes sure $PATH is set correctly by adding DEV_BIN_PATHS to
    'PATH' in |extra_env|.

    Args:
      cmd: command to run. See RemoteAccess.RemoteSh documentation.
      **kwargs: keyword arguments to pass along with cmd. See
        RemoteAccess.RemoteSh documentation.
    """
    extra_env = kwargs.pop('extra_env', {})
    path_env = extra_env.get('PATH', None)
    path_env = self.path if not path_env else '%s:%s' % (path_env, self.path)
    extra_env['PATH'] = path_env
    kwargs['extra_env'] = extra_env
    return super(ChromiumOSDevice, self).RunCommand(cmd, **kwargs)
