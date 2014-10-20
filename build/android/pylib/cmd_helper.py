# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A wrapper for subprocess to make calling shell commands easier."""

import logging
import os
import pipes
import select
import signal
import StringIO
import subprocess
import time

# fcntl is not available on Windows.
try:
  import fcntl
except ImportError:
  fcntl = None


def Popen(args, stdout=None, stderr=None, shell=None, cwd=None, env=None):
  return subprocess.Popen(
      args=args, cwd=cwd, stdout=stdout, stderr=stderr,
      shell=shell, close_fds=True, env=env,
      preexec_fn=lambda: signal.signal(signal.SIGPIPE, signal.SIG_DFL))


def Call(args, stdout=None, stderr=None, shell=None, cwd=None, env=None):
  pipe = Popen(args, stdout=stdout, stderr=stderr, shell=shell, cwd=cwd,
               env=env)
  pipe.communicate()
  return pipe.wait()


def RunCmd(args, cwd=None):
  """Opens a subprocess to execute a program and returns its return value.

  Args:
    args: A string or a sequence of program arguments. The program to execute is
      the string or the first item in the args sequence.
    cwd: If not None, the subprocess's current directory will be changed to
      |cwd| before it's executed.

  Returns:
    Return code from the command execution.
  """
  logging.info(str(args) + ' ' + (cwd or ''))
  return Call(args, cwd=cwd)


def GetCmdOutput(args, cwd=None, shell=False):
  """Open a subprocess to execute a program and returns its output.

  Args:
    args: A string or a sequence of program arguments. The program to execute is
      the string or the first item in the args sequence.
    cwd: If not None, the subprocess's current directory will be changed to
      |cwd| before it's executed.
    shell: Whether to execute args as a shell command.

  Returns:
    Captures and returns the command's stdout.
    Prints the command's stderr to logger (which defaults to stdout).
  """
  (_, output) = GetCmdStatusAndOutput(args, cwd, shell)
  return output


def GetCmdStatusAndOutput(args, cwd=None, shell=False):
  """Executes a subprocess and returns its exit code and output.

  Args:
    args: A string or a sequence of program arguments. The program to execute is
      the string or the first item in the args sequence.
    cwd: If not None, the subprocess's current directory will be changed to
      |cwd| before it's executed.
    shell: Whether to execute args as a shell command.

  Returns:
    The 2-tuple (exit code, output).
  """
  if isinstance(args, basestring):
    args_repr = args
    if not shell:
      raise Exception('string args must be run with shell=True')
  elif shell:
    raise Exception('array args must be run with shell=False')
  else:
    args_repr = ' '.join(map(pipes.quote, args))

  s = '[host]'
  if cwd:
    s += ':' + cwd
  s += '> ' + args_repr
  logging.info(s)
  pipe = Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
               shell=shell, cwd=cwd)
  stdout, stderr = pipe.communicate()

  if stderr:
    logging.critical(stderr)
  if len(stdout) > 4096:
    logging.debug('Truncated output:')
  logging.debug(stdout[:4096])
  return (pipe.returncode, stdout)


class TimeoutError(Exception):
  """Module-specific timeout exception."""
  pass


def GetCmdStatusAndOutputWithTimeout(args, timeout, cwd=None, shell=False,
                                     logfile=None):
  """Executes a subprocess with a timeout.

  Args:
    args: List of arguments to the program, the program to execute is the first
      element.
    timeout: the timeout in seconds or None to wait forever.
    cwd: If not None, the subprocess's current directory will be changed to
      |cwd| before it's executed.
    shell: Whether to execute args as a shell command.
    logfile: Optional file-like object that will receive output from the
      command as it is running.

  Returns:
    The 2-tuple (exit code, output).
  """
  assert fcntl, 'fcntl module is required'
  process = Popen(args, cwd=cwd, shell=shell, stdout=subprocess.PIPE,
                  stderr=subprocess.STDOUT)
  try:
    end_time = (time.time() + timeout) if timeout else None
    poll_interval = 1
    buffer_size = 4096
    child_fd = process.stdout.fileno()
    output = StringIO.StringIO()

    # Enable non-blocking reads from the child's stdout.
    fl = fcntl.fcntl(child_fd, fcntl.F_GETFL)
    fcntl.fcntl(child_fd, fcntl.F_SETFL, fl | os.O_NONBLOCK)

    while True:
      if end_time and time.time() > end_time:
        raise TimeoutError
      read_fds, _, _ = select.select([child_fd], [], [], poll_interval)
      if child_fd in read_fds:
        data = os.read(child_fd, buffer_size)
        if not data:
          break
        if logfile:
          logfile.write(data)
        output.write(data)
      if process.poll() is not None:
        break
  finally:
    try:
      # Make sure the process doesn't stick around if we fail with an
      # exception.
      process.kill()
    except OSError:
      pass
    process.wait()
  return process.returncode, output.getvalue()
