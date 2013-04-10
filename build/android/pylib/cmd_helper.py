# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A wrapper for subprocess to make calling shell commands easier."""

import os
import logging
import signal
import subprocess
import tempfile

import constants


def _Call(args, stdout=None, stderr=None, shell=None, cwd=None):
  return subprocess.call(
      args=args, cwd=cwd, stdout=stdout, stderr=stderr,
      shell=shell, close_fds=True,
      preexec_fn=lambda: signal.signal(signal.SIGPIPE, signal.SIG_DFL))


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
  return _Call(args, cwd=cwd)


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
    The tuple (exit code, output).
  """
  logging.info(str(args) + ' ' + (cwd or ''))
  tmpout = tempfile.TemporaryFile(bufsize=0)
  tmperr = tempfile.TemporaryFile(bufsize=0)
  exit_code = _Call(args, cwd=cwd, stdout=tmpout, stderr=tmperr, shell=shell)
  tmperr.seek(0)
  stderr = tmperr.read()
  tmperr.close()
  if stderr:
    logging.critical(stderr)
  tmpout.seek(0)
  stdout = tmpout.read()
  tmpout.close()
  logging.info(stdout[:4096])  # Truncate output longer than 4k.
  return (exit_code, stdout)


class OutDirectory(object):
  _out_directory = os.path.join(constants.CHROME_DIR, 'out')
  @staticmethod
  def set(out_directory):
    OutDirectory._out_directory = out_directory
  @staticmethod
  def get():
    return OutDirectory._out_directory
