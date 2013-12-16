#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logging related tools."""

import logging
import os
import subprocess
import sys

# Module-level configuration
LOG_FH = None
VERBOSE = False

def SetupLogging(verbose, file_handle=None):
  """Set up python logging.

  Args:
    verbose: If True, log to stderr at DEBUG level and write subprocess output
             to stdout. Otherwise log to stderr at INFO level and do not print
             subprocess output unless there is an error.
    file_handle: If not None, must be a file-like object. All log output will
                 be written at DEBUG level, and all subprocess output will be
                 written, regardless of whether there are errors.
  """
  # Since one of our handlers always wants debug, set the global level to debug.
  logging.getLogger().setLevel(logging.DEBUG)
  stderr_handler = logging.StreamHandler()
  stderr_handler.setFormatter(
      logging.Formatter(fmt='%(levelname)s: %(message)s'))
  if verbose:
    global VERBOSE
    VERBOSE = True
    stderr_handler.setLevel(logging.DEBUG)
  else:
    stderr_handler.setLevel(logging.INFO)
  logging.getLogger().addHandler(stderr_handler)

  if file_handle:
    global LOG_FH
    file_handler = logging.StreamHandler(file_handle)
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(
        logging.Formatter(fmt='%(levelname)s: %(message)s'))
    logging.getLogger().addHandler(file_handler)
    LOG_FH = file_handle

def CheckCall(command, **kwargs):
  """Modulate command output level based on logging level.

  If a logging file handle is set, always emit all output to it.
  If the log level is set at debug or lower, also emit all output to stdout.
  Otherwise, only emit output on error.
  Args:
    command: Command to run.
    **kwargs: Keyword args.
  """
  cwd = os.path.abspath(kwargs.get('cwd', os.getcwd()))
  logging.info('Running: subprocess.check_call(%r, cwd=%r)' %
               (' '.join(command), cwd))
  p = subprocess.Popen(
      command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, **kwargs)

  # Capture the output as it comes and emit it immediately.
  line = p.stdout.readline()
  while line:
    if VERBOSE:
      sys.stdout.write(line)
    if LOG_FH:
      LOG_FH.write(line)
    line = p.stdout.readline()
  if p.wait() != 0:
    raise subprocess.CalledProcessError(cmd=command, returncode=p.returncode)

  # Flush stdout so it does not get interleaved with future log or buildbot
  # output which goes to stderr.
  sys.stdout.flush()
