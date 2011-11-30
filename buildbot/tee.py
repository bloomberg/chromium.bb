# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that handles tee-ing output to a file."""

import errno
import fcntl
import os
import multiprocessing
import select
import sys

_BUFSIZE = 1024

def _output(line, output_files, complain):
  """Print line to output_files.

  Args:
    line: Line to print.
    output_files: List of files to print to.
    complain: Print a warning if we get EAGAIN errors. Only one error
              is printed per line.
  """
  for f in output_files:
    offset = 0
    while offset < len(line):
      select.select([], [f], [])
      try:
        offset += os.write(f.fileno(), line[offset:])
      except OSError as ex:
        if ex.errno == errno.EINTR:
          continue
        elif ex.errno != errno.EAGAIN:
          raise

      if offset < len(line) and complain:
        flags = fcntl.fcntl(f.fileno(), fcntl.F_GETFL, 0)
        if flags & os.O_NONBLOCK:
          warning = '\nWarning: %s/%d is non-blocking.\n' % (f.name,
                                                             f.fileno())
          _output(warning, output_files, False)

        warning = '\nWarning: Short write for %s/%d.\n' % (f.name,
                                                           f.fileno())
        _output(warning, output_files, False)


def _tee(input_file, output_files, complain):
  """Read lines from input_file and write to output_files."""
  for line in iter(lambda: input_file.readline(_BUFSIZE), ''):
    _output(line, output_files, complain)


class _TeeProcess(multiprocessing.Process):
  """Replicate output to multiple file handles."""

  def __init__(self, output_filenames, complain):
    """Write to stdout and supplied filenames.

    Args:
      output_filenames: List of filenames to print to.
      complain: Print a warning if we get EAGAIN errors.
    """

    self._reader_pipe, self.writer_pipe = os.pipe()
    self._output_filenames = output_filenames
    self._complain = complain
    multiprocessing.Process.__init__(self)

  def run(self):
    """Main function for tee subprocess."""

    # Close other end of writer pipe.
    os.close(self.writer_pipe)

    # Read from the pipe.
    input_file = os.fdopen(self._reader_pipe, 'r', 0)

    # Create list of files to write to.
    output_files = [os.fdopen(sys.stdout.fileno(), 'w', 0)]
    for filename in self._output_filenames:
      output_files.append(open(filename, 'w', 0))

    # Read all lines from input_file and write to output_files.
    _tee(input_file, output_files, self._complain)

    # Close input file.
    input_file.close()


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

    # Create a tee subprocess.
    self._tee = _TeeProcess([self._file], True)
    self._tee.start()

    # Redirect stdout and stderr to the tee subprocess.
    writer_pipe = self._tee.writer_pipe
    os.dup2(writer_pipe, sys.stdout.fileno())
    os.dup2(writer_pipe, sys.stderr.fileno())
    os.close(writer_pipe)

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
    self._tee.join()
