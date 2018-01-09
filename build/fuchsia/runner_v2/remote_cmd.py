# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper functions for remotely executing and copying files over a SSH
connection."""

import os
import subprocess

_SSH = ['ssh']
_SCP = ['scp']

COPY_TO_TARGET = 0
COPY_FROM_TARGET = 1


def RunSsh(config_path, host, port, command, silent):
  """Executes an SSH command on the remote host and blocks until completion.

  config_path: Full path to SSH configuration.
  host: The hostname or IP address of the remote host.
  port: The port to connect to.
  command: A list of strings containing the command and its arguments.
  silent: If true, suppresses all output from 'ssh'.

  Returns the exit code from the remote command."""

  ssh_command = _SSH + ['-F', config_path,
                        host,
                        '-p', str(port)] + command
  if silent:
    devnull = open(os.devnull, 'w')
    return subprocess.call(ssh_command, stderr=devnull, stdout=devnull)
  else:
    return subprocess.call(ssh_command)


def RunPipedSsh(config_path, host, port, command):
  """Executes an SSH command on the remote host and returns a process object
  with access to the command's stdio streams. Does not block.

  config_path: Full path to SSH configuration.
  host: The hostname or IP address of the remote host.
  port: The port to connect to.
  command: A list of strings containing the command and its arguments.
  silent: If true, suppresses all output from 'ssh'.

  Returns a Popen object for the command."""

  ssh_command = _SSH + ['-F', config_path,
                        host,
                        '-p', str(port)] + command
  return subprocess.Popen(ssh_command,
                          stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE,
                          stdin=subprocess.PIPE)


def RunScp(config_path, host, port, source, dest, direction):
  """Copies a file to or from a remote host using SCP and blocks until
  completion.

  config_path: Full path to SSH configuration.
  host: The hostname or IP address of the remote host.
  port: The port to connect to.
  source: The path of the file to be copied.
  dest: The path that |source| will be copied to.
  direction: Indicates whether the file should be copied to
             or from the remote side.
             Valid values are COPY_TO_TARGET or COPY_FROM_TARGET.

  Function will raise an assertion if a failure occurred."""

  if direction == COPY_TO_TARGET:
    dest = "%s:%s" % (host, dest)
  else:
    source = "%s:%s" % (host, source)

  scp_command = _SCP + ['-F', config_path,
                        '-P', str(port),
                        source,
                        dest]
  devnull = open('/dev/null', 'w')
  subprocess.check_call(scp_command, stdout=devnull)
