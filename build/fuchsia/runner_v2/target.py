# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import remote_cmd
import subprocess
import sys
import tempfile
import time

_SHUTDOWN_CMD = ['dm', 'poweroff']
_ATTACH_MAX_RETRIES = 10
_ATTACH_RETRY_INTERVAL = 1


class Target(object):
  """Abstract base class representing a Fuchsia deployment target."""

  def __init__(self, output_dir, target_cpu, verbose):
    self._target_cpu = target_cpu
    self._output_dir = output_dir
    self._started = False
    self._dry_run = False
    self._vlogger = sys.stdout if verbose else open(os.devnull, 'w')

  def Start(self):
    """Handles the instantiation and connection process for the Fuchsia
    target instance."""
    pass

  def IsStarted(self):
    """Returns true if the Fuchsia target instance is ready to accept
    commands."""
    return self._started

  def RunCommandPiped(self, command):
    """Starts a remote command and immediately returns a Popen object for the
    command. The caller may interact with the streams, inspect the status code,
    wait on command termination, etc.

    command: A list of strings representing the command and arguments.

    Returns: a Popen object.

    Note: method does not block."""

    self._AssertStarted()
    host, port = self._GetEndpoint()
    return remote_cmd.RunPipedSsh(self._GetSshConfigPath(), host, port, command)

  def RunCommand(self, command, silent=False):
    """Executes a remote command and waits for it to finish executing.

    Returns the exit code of the command."""

    self._AssertStarted()
    host, port = self._GetEndpoint()
    return remote_cmd.RunSsh(self._GetSshConfigPath(), host, port, command,
                             silent)

  def CopyTo(self, source, dest):
    """Copies a file from the local filesystem to the target filesystem.

    source: The path of the file being copied.
    dest: The path on the remote filesystem which will be copied to."""

    self._AssertStarted()
    host, port = self._GetEndpoint()
    command = remote_cmd.RunScp(self._GetSshConfigPath(), host, port,
                                source, dest, remote_cmd.COPY_TO_TARGET)

  def CopyFrom(self, source, dest):
    """Copies a file from the target filesystem to the local filesystem.

    source: The path of the file being copied.
    dest: The path on the local filesystem which will be copied to."""
    self._AssertStarted()
    host, port = self._GetEndpoint()
    return remote_cmd.RunScp(self._GetSshConfigPath(), host, port,
                             source, dest, remote_cmd.COPY_FROM_TARGET)

  def Shutdown(self):
    self.RunCommand(_SHUTDOWN_CMD)
    self._started = False

  def _GetEndpoint(self):
    """Returns a (host, port) tuple for the SSH connection to the target."""
    raise NotImplementedError

  def _GetTargetSdkArch(self):
    """Returns the Fuchsia SDK architecture name for the target CPU."""
    if self._target_cpu == 'arm64':
      return 'aarch64'
    elif self._target_cpu == 'x64':
      return 'x86_64'
    raise Exception('Unknown target_cpu:' + self._target_cpu)

  def _AssertStarted(self):
    assert self.IsStarted()

  def _Attach(self):
    self._vlogger.write('Trying to connect over SSH...')
    self._vlogger.flush()
    for _ in xrange(_ATTACH_MAX_RETRIES):
      host, port = self._GetEndpoint()
      if remote_cmd.RunSsh(self._ssh_config_path, host, port, ['echo'],
                           True) == 0:
        self._vlogger.write(' connected!\n')
        self._vlogger.flush()
        self._started = True
        return
      self._vlogger.write('.')
      self._vlogger.flush()
      time.sleep(_ATTACH_RETRY_INTERVAL)
    sys.stderr.write(' timeout limit reached.\n')
    raise Exception('Couldn\'t connect to QEMU using SSH.')

  def _GetSshConfigPath(self, path):
    raise NotImplementedError

