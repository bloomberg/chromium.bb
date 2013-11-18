# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A wrapper for subprocess to make calling shell commands easier."""

import logging
import pipes
import signal
import subprocess
import tempfile

from utils import timeout_retry


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
  tmpout = tempfile.TemporaryFile(bufsize=0)
  tmperr = tempfile.TemporaryFile(bufsize=0)
  exit_code = Call(args, cwd=cwd, stdout=tmpout, stderr=tmperr, shell=shell)
  tmperr.seek(0)
  stderr = tmperr.read()
  tmperr.close()
  if stderr:
    logging.critical(stderr)
  tmpout.seek(0)
  stdout = tmpout.read()
  tmpout.close()
  if len(stdout) > 4096:
    logging.debug('Truncated output:')
  logging.debug(stdout[:4096])
  return (exit_code, stdout)


def GetCmdStatusAndOutputWithTimeoutAndRetries(args, timeout, retries):
  """Executes a subprocess with a timeout and retries.

  Args:
    args: List of arguments to the program, the program to execute is the first
      element.
    timeout: the timeout in seconds.
    retries: the number of retries.

  Returns:
    The 2-tuple (exit code, output).
  """
  return timeout_retry.Run(GetCmdStatusAndOutput, timeout, retries, [args])
