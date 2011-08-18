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

from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_results as results_lib

_PRINT_INTERVAL = 1

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
    step, output = self._steps.pop(0)
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
    commands.SetNiceness(foreground=False)

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
      except Exception:
        traceback.print_exc(file=output)
        error = traceback.format_exc()

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
