# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utility functions for sdk_update.py and sdk_update_main.py."""

import errno
import os
import shutil
import subprocess
import sys
import time


class Error(Exception):
  """Generic error/exception for sdk_update module"""
  pass


def RemoveDir(outdir):
  """Removes the given directory

  On Unix systems, this just runs shutil.rmtree, but on Windows, this doesn't
  work when the directory contains junctions (as does our SDK installer).
  Therefore, on Windows, it runs rmdir /S /Q as a shell command.  This always
  does the right thing on Windows. If the directory already didn't exist,
  RemoveDir will return successfully without taking any action.

  Args:
    outdir: The directory to delete

  Raises:
    CalledProcessError - if the delete operation fails on Windows
    OSError - if the delete operation fails on Linux
  """

  try:
    shutil.rmtree(outdir)
  except OSError:
    if not os.path.exists(outdir):
      return
    # On Windows this could be an issue with junctions, so try again with rmdir
    if sys.platform == 'win32':
      subprocess.check_call(['rmdir', '/S', '/Q', outdir], shell=True)


def RenameDir(srcdir, destdir):
  """Renames srcdir to destdir. Removes destdir before doing the
     rename if it already exists."""

  max_tries = 5
  num_tries = 0
  for num_tries in xrange(max_tries):
    try:
      RemoveDir(destdir)
      os.rename(srcdir, destdir)
      return
    except OSError as err:
      if err.errno != errno.EACCES:
        raise err
      # If we are here, we didn't exit due to raised exception, so we are
      # handling a Windows flaky access error.  Sleep one second and try
      # again.
      time.sleep(num_tries + 1)

  # end of while loop -- could not RenameDir
  raise Error('Could not RenameDir %s => %s after %d tries.\n'
              'Please check that no shells or applications '
              'are accessing files in %s.'
              % (srcdir, destdir, num_tries, destdir))
