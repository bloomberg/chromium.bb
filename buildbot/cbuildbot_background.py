# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for running cbuildbot stages in the background."""

import multiprocessing
import os
import Queue
import sys
import tempfile
import traceback

from chromite.buildbot import builderstage as bs
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.lib import cros_build_lib as cros_lib

_PRINT_INTERVAL = 1


def SetNiceness(foreground):
  """Set the niceness of this process.

  Args:
    foreground: If set, the process runs with higher priority. This means
    that the process will be scheduled more often when accessing resources
    (e.g. cpu and disk).
  """
  # Note: -c 2 means best effort priority.
  pid_str = str(os.getpid())
  ionice_cmd = ['ionice', '-p', pid_str, '-c', '2']
  renice_cmd = ['sudo', 'renice']
  if foreground:
    # Set this program to foreground priority. ionice and negative niceness
    # is honored by sudo and passed to subprocesses.
    ionice_cmd.extend(['-n', '0'])
    renice_cmd.extend(['-n', '-20', '-p', pid_str])
  else:
    # Set this program to background priority. Positive niceness isn't
    # inherited by sudo, so we just set to zero.
    ionice_cmd.extend(['-n', '7'])
    renice_cmd.extend(['-n', '0', '-p', pid_str])
  cros_lib.RunCommand(ionice_cmd, print_cmd=False)
  cros_lib.RunCommand(renice_cmd, print_cmd=False, redirect_stdout=True)


class BackgroundException(Exception):
  pass

class BackgroundSteps(multiprocessing.Process):
  """Run a list of functions in sequence in the background.

  These functions may be the 'Run' functions from buildbot stages or just plain
  functions. They will be run in the background. Output from these functions
  is saved to a temporary file and is printed when the 'WaitForStep' function
  is called.
  """

  def __init__(self):
    multiprocessing.Process.__init__(self)
    self._steps = []
    self._queue = multiprocessing.Queue()

  def AddStep(self, step):
    """Add a step to the list of steps to run in the background."""
    output = tempfile.NamedTemporaryFile(delete=False)
    self._steps.append((step, output))

  def WaitForStep(self):
    """Wait for the next step to complete.

    Output from the step is printed as the step runs.

    If an exception occurs, return a string containing the traceback.
    """
    assert not self.Empty()
    _step, output = self._steps.pop(0)
    pos = 0
    more_output = True
    while more_output:
      # Check whether the process is finished.
      try:
        error, results = self._queue.get(True, _PRINT_INTERVAL)
        more_output = False
      except Queue.Empty:
        more_output = True

      # Print output so far.
      output.seek(pos)
      for line in output:
        sys.stdout.write(line)
      pos = output.tell()

    # Cleanup temp file.
    output.close()
    os.unlink(output.name)

    # Propagate any results.
    for result in results:
      results_lib.Results.Record(*result)

    # If a traceback occurred, return it.
    return error

  def Empty(self):
    """Return True if there are any steps left to run."""
    return len(self._steps) == 0

  def run(self):
    """Run the list of steps."""

    # Be nice so that foreground processes get CPU if they need it.
    SetNiceness(foreground=False)

    stdout_fileno = sys.stdout.fileno()
    stderr_fileno = sys.stderr.fileno()
    for step, output in self._steps:
      # Send all output to a named temporary file.
      os.dup2(output.fileno(), stdout_fileno)
      os.dup2(output.fileno(), stderr_fileno)
      error = None
      try:
        results_lib.Results.Clear()
        step()
      except bs.NonBacktraceBuildException:
        error = traceback.format_exc()
      except Exception:
        traceback.print_exc(file=sys.stderr)
        error = traceback.format_exc()

      sys.stdout.flush()
      sys.stderr.flush()
      output.close()
      results = results_lib.Results.Get()
      self._queue.put((error, results))


def RunParallelSteps(steps):
  """Run a list of functions in parallel.

  The output from the functions is saved to a temporary file and printed as if
  they were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel steps have finished running.
  """
  # First, start all the steps.
  bg_steps = []
  for step in steps:
    bg = BackgroundSteps()
    bg.AddStep(step)
    bg.start()
    bg_steps.append(bg)

  # Wait for each step to complete.
  tracebacks = []
  for bg in bg_steps:
    while not bg.Empty():
      error = bg.WaitForStep()
      if error:
        tracebacks.append(error)
    bg.join()

  # Propagate any exceptions.
  if tracebacks:
    raise BackgroundException('\n' + ''.join(tracebacks))


def RunTasksInProcessPool(task, inputs, processes=None):
  """Run the specified function with each supplied input in a pool of processes.

  This function runs task(*x) for x in inputs in a pool of processes. The
  output from these tasks is saved to a temporary file and printed as if they
  were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel steps have finished running.

  Args:
    processes: Number of processes, at most, to launch.
  """

  def TaskRunner():
    while True:
      try:
        x = queue.get_nowait()
      except Queue.Empty:
        return
      task(*x)

  if not processes:
    processes = min(multiprocessing.cpu_count(), len(inputs))

  queue = multiprocessing.Queue()
  for x in inputs:
    queue.put(x)

  RunParallelSteps([TaskRunner] * processes)
