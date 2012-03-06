# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for running cbuildbot stages in the background."""

import contextlib
import functools
import multiprocessing
import os
import Queue
import sys
import tempfile
import traceback

from chromite.buildbot import builderstage as bs
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


@contextlib.contextmanager
def _ParallelSteps(steps):
  """Run a list of functions in parallel.

  This function launches the provided functions in the background, yields,
  and then waits for the functions to exit.

  The output from the functions is saved to a temporary file and printed as if
  they were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel tasks have finished running. Further, a
  BackgroundException is raised with full stack traces of all exceptions.
  """

  # First, start all the steps.
  bg_steps = []
  for step in steps:
    bg = BackgroundSteps()
    bg.AddStep(step)
    bg.start()
    bg_steps.append(bg)

  try:
    yield
  finally:
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


def RunParallelSteps(steps):
  """Run a list of functions in parallel.

  The output from the functions is saved to a temporary file and printed as if
  they were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel tasks have finished running. Further, a
  BackgroundException is raised with full stack traces of all exceptions.
  """
  with _ParallelSteps(steps):
    pass


class _AllTasksComplete(object):
  """Sentinel object to indicate that all tasks are complete."""


def _TaskRunner(queue, task, onexit=None):
  """Run task(*input) for each input in the queue.

  Returns when it encounters an _AllTasksComplete object on the queue.
  If exceptions occur, save them off and re-raise them as a
  BackgroundException once we've finished processing the items in the queue.

  Args:
    queue: A queue of tasks to run. Add tasks to this queue, and they will
      be run.
    task: Function to run on each queued input.
    onexit: Function to run after all inputs are processed.
  """
  tracebacks = []
  while True:
    # Wait for a new item to show up on the queue. This is a blocking wait,
    # so if there's nothing to do, we just sit here.
    x = queue.get()
    if isinstance(x, _AllTasksComplete):
      # All tasks are complete, so we should exit.
      queue.put(x)
      break

    # If no tasks failed yet, process the remaining tasks.
    if not tracebacks:
      try:
        task(*x)
      except Exception:
        tracebacks.append(traceback.format_exc())

  # Run exit handlers.
  if onexit:
    onexit()

  # Propagate any exceptions.
  if tracebacks:
    raise BackgroundException('\n' + ''.join(tracebacks))


@contextlib.contextmanager
def BackgroundTaskRunner(queue, task, processes=None, onexit=None):
  """Run the specified task on each queued input in a pool of processes.

  This context manager starts a set of workers in the background, who each
  wait for input on the specified queue. These workers run task(*input) for
  each input on the queue.

  The output from these tasks is saved to a temporary file. When control
  returns to the context manager, the background output is printed in order,
  as if the tasks were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel tasks have finished running. Further, a
  BackgroundException is raised with full stack traces of all exceptions.

  Args:
    queue: A queue of tasks to run. Add tasks to this queue, and they will
      be run in the background.
    task: Function to run on each queued input.
    processes: Number of processes to launch.
    onexit: Function to run in each background process after all inputs are
      processed.
  """

  if not processes:
    processes = multiprocessing.cpu_count()

  steps = [functools.partial(_TaskRunner, queue, task, onexit)] * processes
  with _ParallelSteps(steps):
    try:
      yield
    finally:
      queue.put(_AllTasksComplete())


def RunTasksInProcessPool(task, inputs, processes=None):
  """Run the specified function with each supplied input in a pool of processes.

  This function runs task(*x) for x in inputs in a pool of processes. The
  output from these tasks is saved to a temporary file and printed as if they
  were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel tasks have finished running. Further, a
  BackgroundException is raised with full stack traces of all exceptions.

  Args:
    task: Function to run on each input.
    inputs: List of inputs.
    processes: Number of processes, at most, to launch.
  """

  if not processes:
    processes = min(multiprocessing.cpu_count(), len(inputs))

  queue = multiprocessing.Queue()
  with BackgroundTaskRunner(queue, task, processes):
    for x in inputs:
      queue.put(x)
