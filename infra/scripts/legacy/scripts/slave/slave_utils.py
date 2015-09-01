# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions specific to build slaves, shared by several buildbot scripts.
"""

import datetime
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

from common import chromium_utils

# These codes used to distinguish true errors from script warnings.
ERROR_EXIT_CODE = 1
WARNING_EXIT_CODE = 88


def SlaveBuildName(chrome_dir):
  """Extracts the build name of this slave (e.g., 'chrome-release') from the
  leaf subdir of its build directory.
  """
  return os.path.basename(SlaveBaseDir(chrome_dir))


def SlaveBaseDir(chrome_dir):
  """Finds the full path to the build slave's base directory (e.g.
  'c:/b/chrome/chrome-release').  This is assumed to be the parent of the
  shallowest 'build' directory in the chrome_dir path.

  Raises chromium_utils.PathNotFound if there is no such directory.
  """
  result = ''
  prev_dir = ''
  curr_dir = chrome_dir
  while prev_dir != curr_dir:
    (parent, leaf) = os.path.split(curr_dir)
    if leaf == 'build':
      # Remember this one and keep looking for something shallower.
      result = parent
    if leaf == 'slave':
      # We are too deep, stop now.
      break
    prev_dir = curr_dir
    curr_dir = parent
  if not result:
    raise chromium_utils.PathNotFound('Unable to find slave base dir above %s' %
                                      chrome_dir)
  return result


def WriteLogLines(logname, lines, perf=None):
  logname = logname.rstrip()
  lines = [line.rstrip() for line in lines]
  for line in lines:
    print '@@@STEP_LOG_LINE@%s@%s@@@' % (logname, line)
  if perf:
    perf = perf.rstrip()
    print '@@@STEP_LOG_END_PERF@%s@%s@@@' % (logname, perf)
  else:
    print '@@@STEP_LOG_END@%s@@@' % logname
