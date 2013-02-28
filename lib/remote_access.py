#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library containing functions to access a remote test device."""

import logging
import os
import shutil
import stat
import time

from chromite.lib import cros_build_lib


_path = os.path.dirname(os.path.realpath(__file__))
TEST_PRIVATE_KEY = os.path.normpath(
    os.path.join(_path, '../ssh_keys/testing_rsa'))
del _path

REBOOT_MARKER = '/tmp/awaiting_reboot'
REBOOT_MAX_WAIT = 120
REBOOT_SSH_CONNECT_TIMEOUT = 2
REBOOT_SSH_CONNECT_ATTEMPTS = 2
CHECK_INTERVAL = 5
DEFAULT_SSH_PORT = 22
SSH_ERROR_CODE = 255


def CompileSSHConnectSettings(ConnectTimeout=30, ConnectionAttempts=4):
  return ['-o', 'ConnectTimeout=%s' % ConnectTimeout,
          '-o', 'ConnectionAttempts=%s' % ConnectionAttempts,
          '-o', 'NumberOfPasswordPrompts=0',
          '-o', 'Protocol=2',
          '-o', 'ServerAliveInterval=10',
          '-o', 'ServerAliveCountMax=3',
          '-o', 'StrictHostKeyChecking=no',
          '-o', 'UserKnownHostsFile=/dev/null',]


class RemoteAccess(object):
  """Provides access to a remote test machine."""

  def __init__(self, remote_host, tempdir, port=DEFAULT_SSH_PORT,
               debug_level=logging.DEBUG):
    """Construct the object.

    Arguments:
      remote_host: The ip or hostname of the remote test machine.  The test
                   machine should be running a ChromeOS test image.
      tempdir: A directory that RemoteAccess can use to store temporary files.
               It's the responsibility of the caller to remove it.
      port: The ssh port of the test machine to connect to.
      debug_level: Logging level to use for all RunCommand invocations.
    """
    self.tempdir = tempdir
    self.remote_host = remote_host
    self.port = port
    self.debug_level = debug_level
    self.private_key = os.path.join(tempdir, os.path.basename(TEST_PRIVATE_KEY))
    shutil.copyfile(TEST_PRIVATE_KEY, self.private_key)
    os.chmod(self.private_key, stat.S_IRUSR)

  @property
  def target_ssh_url(self):
    return 'root@%s' % self.remote_host

  def _GetSSHCmd(self, connect_settings=None):
    if connect_settings is None:
      connect_settings = CompileSSHConnectSettings()

    return (['ssh', '-p', str(self.port)] +
             connect_settings +
             ['-i', self.private_key, ])

  def RemoteSh(self, cmd, connect_settings=None, error_code_ok=False,
               ssh_error_ok=False, debug_level=None):
    """Run a sh command on the remote device through ssh.

    Arguments:
      cmd: The command string to run. *not a list!*
      connect_settings: The SSH connect settings to use.
      error_code_ok: Does not throw an exception when the command exits with a
                     non-zero returncode.  This does not cover the case where
                     the ssh command itself fails (return code 255).
                     See ssh_error_ok.
      ssh_error_ok: Does not throw an exception when the ssh command itself
                    fails (return code 255).
      debug_level:  See cros_build_lib.RunCommand documentation.

    Returns:
      A CommandResult object.  The returncode is the returncode of the command,
      or 255 if ssh encountered an error (could not connect, connection
      interrupted, etc.)

    Raises:  RunCommandError when error is not ignored through error_code_ok and
             ssh_error_ok flags.
    """
    if not debug_level:
      debug_level = self.debug_level
    ssh_cmd = self._GetSSHCmd(connect_settings)
    ssh_cmd += [self.target_ssh_url, cmd]
    try:
      result = cros_build_lib.RunCommandCaptureOutput(
          ssh_cmd, debug_level=debug_level)
    except cros_build_lib.RunCommandError as e:
      if ((e.result.returncode == SSH_ERROR_CODE and ssh_error_ok) or
          (e.result.returncode and e.result.returncode != SSH_ERROR_CODE
           and error_code_ok)):
        result = e.result
      else:
        raise

    return result

  def LearnBoard(self):
    """Grab the board reported by the remote device.

    in the case of multiple matches, uses the first one.  In the case of no
    entry, returns an empty string.
    """
    result = self.RemoteSh('grep CHROMEOS_RELEASE_BOARD /etc/lsb-release')
    # In the case of multiple matches, use the first one.
    output = result.output.splitlines()
    if len(output) > 1:
      logging.debug('More than one board entry found!  Using the first one.')

    return output[0].strip().partition('=')[-1]

  def _CheckIfRebooted(self):
    """"Checks whether a remote device has rebooted successfully.

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
                           error_code_ok=True, ssh_error_ok=True)

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
    self.RemoteSh('touch %s && reboot' % REBOOT_MARKER)
    time.sleep(CHECK_INTERVAL)
    try:
      cros_build_lib.WaitForCondition(self._CheckIfRebooted, CHECK_INTERVAL,
                                      REBOOT_MAX_WAIT)
    except cros_build_lib.TimeoutError:
      cros_build_lib.Die('Reboot has not completed after %s seconds; giving up.'
                         % (REBOOT_MAX_WAIT,))

  def Rsync(self, src, dest, inplace=False, debug_level=None, sudo=False):
    """Rsync a directory to the remote device.

    Arguments:
      src: The local src directory.
      dest: The remote dest directory.
      inplace: If set, cause rsync to overwrite the dest files in place.  This
               conserves space, but has some side effects - see rsync man page.
      debug_level: See cros_build_lib.RunCommand documentation.
      sudo: If set, invoke the command via sudo.
    """
    if not debug_level:
      debug_level = self.debug_level

    ssh_cmd = ' '.join(self._GetSSHCmd())
    rsync_cmd = ['rsync', '--recursive', '--links', '--perms',  '--verbose',
                 '--compress']
    # In cases where the developer sets up a ssh daemon manually on a device
    # with a dev image, the ssh login $PATH can be incorrect, and the target
    # rsync will not be found.  So we try to provide the right $PATH here.
    rsync_cmd += ['--rsync-path', 'PATH=/usr/local/bin:$PATH rsync']
    if inplace:
      rsync_cmd.append('--inplace')
    rsync_cmd += ['--progress', '--rsh', ssh_cmd, src,
                  '%s:%s' % (self.target_ssh_url, dest)]
    rc_func = cros_build_lib.RunCommand
    if sudo:
      rc_func = cros_build_lib.SudoRunCommand
    return rc_func(rsync_cmd, debug_level=debug_level)
