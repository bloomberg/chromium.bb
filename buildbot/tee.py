# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles tee-ing output to a file."""

import os
import subprocess
import sys


class Tee(object):
  """Class that handles tee-ing output to a file."""
  def __init__(self, file):
    """Initializes object with path to log file."""
    self._file = file
    self._old_stdout = None
    self._old_stderr = None
    self._tee = None

  def start(self):
    """Start tee-ing all stdout and stderr output to the file."""
    # Flush and save old file descriptors.
    sys.stdout.flush()
    sys.stderr.flush()
    self._old_stdout = os.dup(sys.stdout.fileno())
    self._old_stderr = os.dup(sys.stdout.fileno())

    # Create a tee subprocess and redirect stdout and stderr to it.
    self._tee = subprocess.Popen(['tee', self._file], stdin=subprocess.PIPE)
    os.dup2(self._tee.stdin.fileno(), sys.stdout.fileno())
    os.dup2(self._tee.stdin.fileno(), sys.stderr.fileno())

  def stop(self):
    """Restores old stdout and stderr handles and waits for tee proc to exit."""
    sys.stdout.flush()
    sys.stderr.flush()
    self._tee.stdin.flush()

    # Use os.close() to break the link from the sys.stdout file descriptor to
    # the tee process's stdin.  Ditto for sys.stderr.  We don't want to call
    # sys.std[out|err].close() because that will close the file descriptor.
    # We still want to redirect the sys.std[out|err] fd to the console.
    os.close(sys.stdout.fileno())
    os.close(sys.stderr.fileno())
    self._tee.stdin.close()

    # Restore old file descriptors.
    os.dup2(self._old_stdout, sys.stdout.fileno())
    os.dup2(self._old_stderr, sys.stderr.fileno())
    self._tee.wait()
