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
    self._old_stdout_fd = None
    self._old_stderr_fd = None
    self._tee = None

  def start(self):
    """Start tee-ing all stdout and stderr output to the file."""
    # Flush and save old file descriptors.
    sys.stdout.flush()
    sys.stderr.flush()
    self._old_stdout_fd = os.dup(sys.stdout.fileno())
    self._old_stderr_fd = os.dup(sys.stderr.fileno())
    # Save file objects
    self._old_stdout = sys.stdout
    self._old_stderr = sys.stderr

    # Replace std[out|err] with unbuffered file objects
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)
    sys.stderr = os.fdopen(sys.stderr.fileno(), 'w', 0)

    # Create a tee subprocess and redirect stdout and stderr to it.
    self._tee = subprocess.Popen(['tee', self._file], stdin=subprocess.PIPE,
      close_fds=True)
    os.dup2(self._tee.stdin.fileno(), sys.stdout.fileno())
    os.dup2(self._tee.stdin.fileno(), sys.stderr.fileno())
    self._tee.stdin.close()

  def stop(self):
    """Restores old stdout and stderr handles and waits for tee proc to exit."""
    # Close unbuffered std[out|err] file objects, as well as the tee's stdin.
    sys.stdout.close()
    sys.stderr.close()

    # Restore file objects
    sys.stdout = self._old_stdout
    sys.stderr = self._old_stderr

    # Restore old file descriptors.
    os.dup2(self._old_stdout_fd, sys.stdout.fileno())
    os.dup2(self._old_stderr_fd, sys.stderr.fileno())
    os.close(self._old_stdout_fd)
    os.close(self._old_stderr_fd)
    self._tee.wait()
