#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Logging related tools."""

import logging
import os
import subprocess
import sys


def CheckCall(command, **kwargs):
  """Modulate command output level based on logging level.

  For debug or more verbose, emit output immediately.
  Otherwise, only emit output on error.
  Args:
    command: Command to run.
    **kwargs: Keyword args.
  """
  cwd = os.path.abspath(kwargs.get('cwd', os.getcwd()))
  logging.info('Running: subprocess.check_call(%r, cwd=%r)' % (command, cwd))
  # Rather than directing subprocess output to logging,
  # querying the logging level to decide if we emit command output or
  # capture and emit output only on error.
  if logging.getLogger().getEffectiveLevel() <= logging.DEBUG:
    subprocess.check_call(command, **kwargs)
  else:
    p = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, **kwargs)
    p_stdout, p_stderr = p.communicate()
    if p.wait() != 0:
      sys.stdout.write(p_stdout)
      sys.stderr.write(p_stderr)
      raise subprocess.CalledProcessError(cmd=command, returncode=p.returncode)
