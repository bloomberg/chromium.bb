# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Contains a helper function for deploying and executing a packaged
executable on a Target."""

import common
import json
import logging
import multiprocessing
import os
import shutil
import subprocess
import tempfile
import threading
import uuid
import select

from symbolizer import FilterStream

FAR = os.path.join(common.SDK_ROOT, 'tools', 'far')
PM = os.path.join(common.SDK_ROOT, 'tools', 'pm')

# Amount of time to wait for the termination of the system log output thread.
_JOIN_TIMEOUT_SECS = 5


def _AttachKernelLogReader(target):
  """Attaches a kernel log reader as a long-running SSH task."""

  logging.info('Attaching kernel logger.')
  return target.RunCommandPiped(['dlog', '-f'], stdin=open(os.devnull, 'r'),
                                stdout=subprocess.PIPE)


def _ReadMergedLines(streams):
  """Creates a generator which merges the buffered line output from |streams|.
  The generator is terminated when the primary (first in sequence) stream
  signals EOF. Absolute output ordering is not guaranteed."""

  assert len(streams) > 0
  poll = select.poll()
  streams_by_fd = {}
  primary_fd = streams[0].fileno()
  for s in streams:
    poll.register(s.fileno(), select.POLLIN)
    streams_by_fd[s.fileno()] = s

  try:
    while primary_fd != None:
      events = poll.poll(1)
      for fileno, event in events:
        if event & select.POLLIN:
          yield streams_by_fd[fileno].readline()

        elif event & select.POLLHUP:
          poll.unregister(fileno)
          del streams_by_fd[fileno]

          if fileno == primary_fd:
            primary_fd = None
  finally:
    for fd_to_cleanup, _ in streams_by_fd.iteritems():
      poll.unregister(fd_to_cleanup)


def DrainStreamToStdout(stream, quit_event):
  """Outputs the contents of |stream| until |quit_event| is set."""

  poll = select.poll()
  poll.register(stream.fileno(), select.POLLIN)
  try:
    while not quit_event.is_set():
      events = poll.poll(1)
      for fileno, event in events:
        if event & select.POLLIN:
          print stream.readline().rstrip()
        elif event & select.POLLHUP:
          break

  finally:
    poll.unregister(stream.fileno())


def RunPackage(output_dir, target, package_path, package_name, run_args,
               system_logging, symbolizer_config=None):
  """Copies the Fuchsia package at |package_path| to the target,
  executes it with |run_args|, and symbolizes its output.

  output_dir: The path containing the build output files.
  target: The deployment Target object that will run the package.
  package_path: The path to the .far package file.
  package_name: The name of app specified by package metadata.
  run_args: The arguments which will be passed to the Fuchsia process.
  system_logging: If true, connects a system log reader to the target.
  symbolizer_config: A newline delimited list of source files contained
                     in the package. Omitting this parameter will disable
                     symbolization.

  Returns the exit code of the remote package process."""


  system_logger = _AttachKernelLogReader(target) if system_logging else None
  package_copied = False
  try:
    if system_logger:
      # Spin up a thread to asynchronously dump the system log to stdout
      # for easier diagnoses of early, pre-execution failures.
      log_output_quit_event = multiprocessing.Event()
      log_output_thread = threading.Thread(
          target=lambda: DrainStreamToStdout(system_logger.stdout,
                                             log_output_quit_event))
      log_output_thread.daemon = True
      log_output_thread.start()

    logging.info('Copying package to target.')
    install_path = os.path.join('/data', os.path.basename(package_path))
    target.PutFile(package_path, install_path)
    package_copied = True

    logging.info('Installing package.')
    p = target.RunCommandPiped(['pm', 'install', install_path],
                               stderr=subprocess.PIPE)
    output = p.stderr.readlines()
    p.wait()

    if p.returncode != 0:
      # Don't error out if the package already exists on the device.
      if len(output) != 1 or 'ErrAlreadyExists' not in output[0]:
        raise Exception('Error while installing: %s' % '\n'.join(output))

    if system_logger:
      log_output_quit_event.set()
      log_output_thread.join(timeout=_JOIN_TIMEOUT_SECS)

    logging.info('Running application.')
    command = ['run', package_name] + run_args
    process = target.RunCommandPiped(command,
                                     stdin=open(os.devnull, 'r'),
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT)

    if system_logger:
      task_output = _ReadMergedLines([process.stdout, system_logger.stdout])
    else:
      task_output = process.stdout

    if symbolizer_config:
      # Decorate the process output stream with the symbolizer.
      output = FilterStream(task_output, package_name, symbolizer_config,
                            output_dir)
    else:
      logging.warn('Symbolization is DISABLED.')
      output = process.stdout

    for next_line in output:
      print next_line.rstrip()

    process.wait()
    if process.returncode == 0:
      logging.info('Process exited normally with status code 0.')
    else:
      # The test runner returns an error status code if *any* tests fail,
      # so we should proceed anyway.
      logging.warning('Process exited with status code %d.' %
                      process.returncode)

  finally:
    if system_logger:
      logging.info('Terminating kernel log reader.')
      log_output_quit_event.set()
      log_output_thread.join()
      system_logger.kill()

    if package_copied:
      logging.info('Removing package source from device.')
      target.RunCommand(['rm', install_path])


  return process.returncode
