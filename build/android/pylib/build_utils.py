# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import fnmatch
import os
import pipes
import shlex
import shutil
import subprocess
import sys
import traceback


def MakeDirectory(dir_path):
  try:
    os.makedirs(dir_path)
  except OSError:
    pass


def DeleteDirectory(dir_path):
  if os.path.exists(dir_path):
    shutil.rmtree(dir_path)


def Touch(path):
  MakeDirectory(os.path.dirname(path))
  with open(path, 'a'):
    os.utime(path, None)


def FindInDirectory(directory, filter):
  files = []
  for root, dirnames, filenames in os.walk(directory):
    matched_files = fnmatch.filter(filenames, filter)
    files.extend((os.path.join(root, f) for f in matched_files))
  return files


def FindInDirectories(directories, filter):
  all_files = []
  for directory in directories:
    all_files.extend(FindInDirectory(directory, filter))
  return all_files


def ParseGypList(gyp_string):
  # The ninja generator doesn't support $ in strings, so use ## to
  # represent $.
  # TODO(cjhopman): Remove when
  # https://code.google.com/p/gyp/issues/detail?id=327
  # is addressed.
  gyp_string = gyp_string.replace('##', '$')
  return shlex.split(gyp_string)


# This can be used in most cases like subprocess.check_call. The output,
# particularly when the command fails, better highlights the command's failure.
# This call will directly exit on a failure in the subprocess so that no python
# stacktrace is printed after the output of the failed command.
def CheckCallDie(args, cwd=None):
  if not cwd:
    cwd = os.getcwd()

  child = subprocess.Popen(args,
      stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=cwd)

  stdout, _ = child.communicate()

  if child.returncode:
    stacktrace = traceback.extract_stack()
    print >> sys.stderr, ''.join(traceback.format_list(stacktrace))
    # A user should be able to simply copy and paste the command that failed
    # into their shell.
    copyable_command = ' '.join(map(pipes.quote, args))
    copyable_command = ('( cd ' + os.path.abspath(cwd) + '; '
        + copyable_command + ' )')
    print >> sys.stderr, 'Command failed:', copyable_command, '\n'

    if stdout:
      print stdout,

    # Directly exit to avoid printing stacktrace.
    sys.exit(child.returncode)

  else:
    if stdout:
      print stdout,

