# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Common python commands used by various build scripts."""

import os
import subprocess
import sys

_STDOUT_IS_TTY = hasattr(sys.stdout, 'isatty') and sys.stdout.isatty()

def RunCommand(cmd, print_cmd=True, error_ok=False, error_message=None,
               exit_code=False, redirect_stdout=False, redirect_stderr=False,
               cwd=None, input=None, enter_chroot=False, shell=False):
  """Runs a command.

  Keyword arguments:
    cmd - cmd to run.  Should be input to subprocess.Popen.
    print_cmd -- prints the command before running it.
    error_ok -- does not raise an exception on error.
    error_message -- prints out this message when an error occurrs.
    exit_code -- returns the return code of the shell command.
    redirect_stdout -- returns the stdout.
    redirect_stderr -- holds stderr output until input is communicated.
    cwd -- the working directory to run this cmd.
    input -- input to pipe into this command through stdin.
    enter_chroot -- this command should be run from within the chroot.  If set,
      cwd must point to the scripts directory.
    shell -- If shell is True, the specified command will be executed through the shell.
  Raises:
    Exception:  Raises generic exception on error with optional error_message.
  """
  # Set default for variables.
  stdout = None
  stderr = None
  stdin = None
  output = ''

  # Modify defaults based on parameters.
  if redirect_stdout:  stdout = subprocess.PIPE
  if redirect_stderr:  stderr = subprocess.PIPE
  if input:  stdin = subprocess.PIPE
  if isinstance(cmd, basestring):
    if enter_chroot: cmd = './enter_chroot.sh -- ' + cmd
    cmd_str = cmd
  else:
    if enter_chroot: cmd = ['./enter_chroot.sh', '--'] + cmd
    cmd_str = ' '.join(cmd)

  # Print out the command before running.
  if print_cmd:
    Info('RunCommand: %s' % cmd_str)

  try:
    proc = subprocess.Popen(cmd, cwd=cwd, stdin=stdin,
                            stdout=stdout, stderr=stderr,
                            shell=shell)
    (output, error) = proc.communicate(input)
    if exit_code:
      return proc.returncode

    if not error_ok and proc.returncode:
      raise Exception('Command "%s" failed.\n' % cmd_str +
                      (error_message or error or output or ''))
  except Exception,e:
    if not error_ok:
      raise
    else:
      Warning(str(e))

  return output


class Color(object):
  """Conditionally wraps text in ANSI color escape sequences."""
  BLACK, RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE = range(8)
  BOLD = -1
  COLOR_START = '\033[1;%dm'
  BOLD_START = '\033[1m'
  RESET = '\033[0m'

  def __init__(self, enabled=True):
    self._enabled = enabled

  def Color(self, color, text):
    """Returns text with conditionally added color escape sequences.

    Keyword arguments:
      color: Text color -- one of the color constants defined in this class.
      text: The text to color.

    Returns:
      If self._enabled is False, returns the original text. If it's True,
      returns text with color escape sequences based on the value of color.
    """
    if not self._enabled:
      return text
    if color == self.BOLD:
      start = self.BOLD_START
    else:
      start = self.COLOR_START % (color + 30)
    return start + text + self.RESET


def Die(message):
  """Emits a red error message and halts execution.

  Keyword arguments:
    message: The message to be emitted before exiting.
  """
  print >> sys.stderr, (
      Color(_STDOUT_IS_TTY).Color(Color.RED, '\nERROR: ' + message))
  sys.exit(1)


def Warning(message):
  """Emits a yellow warning message and continues execution.

  Keyword arguments:
    message: The message to be emitted.
  """
  print >> sys.stderr, (
      Color(_STDOUT_IS_TTY).Color(Color.YELLOW, '\nWARNING: ' + message))


def Info(message):
  """Emits a blue informational message and continues execution.

  Keyword arguments:
    message: The message to be emitted.
  """
  print >> sys.stderr, (
      Color(_STDOUT_IS_TTY).Color(Color.BLUE, '\nINFO: ' + message))


def ListFiles(base_dir):
  """Recurively list files in a directory.

  Keyword arguments:
    base_dir: directory to start recursively listing in.

  Returns:
    A list of files relative to the base_dir path or
    An empty list of there are no files in the directories.
  """
  directories = [base_dir]
  files_list = []
  while directories:
    directory = directories.pop()
    for name in os.listdir(directory):
      fullpath = os.path.join(directory, name)
      if os.path.isfile(fullpath):
        files_list.append(fullpath)
      elif os.path.isdir(fullpath):
        directories.append(fullpath)

  return files_list
