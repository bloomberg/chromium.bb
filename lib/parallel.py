# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for running cbuildbot stages in the background."""

import collections
import contextlib
import functools
import multiprocessing
import os
import Queue
import sys
import tempfile
import traceback

from chromite.buildbot import cbuildbot_results as results_lib

_PRINT_INTERVAL = 1
_BUFSIZE = 1024


class BackgroundFailure(results_lib.StepFailure):
  pass


class _BackgroundSteps(multiprocessing.Process):
  """Run a list of functions in sequence in the background.

  These functions may be the 'Run' functions from buildbot stages or just plain
  functions. They will be run in the background. Output from these functions
  is saved to a temporary file and is printed when the 'WaitForStep' function
  is called.
  """

  def __init__(self, semaphore=None):
    """Create a new _BackgroundSteps object.

    If semaphore is supplied, it will be acquired for the duration of the
    steps that are run in the background. This can be used to limit the
    number of simultaneous parallel tasks.
    """
    multiprocessing.Process.__init__(self)
    self._steps = collections.deque()
    self._queue = multiprocessing.Queue()
    self._semaphore = semaphore

  def AddStep(self, step):
    """Add a step to the list of steps to run in the background."""
    output = tempfile.NamedTemporaryFile(delete=False, bufsize=0)
    self._steps.append((step, output))

  def WaitForStep(self, silent=False):
    """Wait for the next step to complete.

    Output from the step is printed as the step runs.

    If an exception occurs, return a string containing the traceback.

    Arguments:
      silent: If True, squelch all output from the step.
    """
    assert not self.Empty()
    _step, output = self._steps.popleft()

    # Flush stdout and stderr to be sure no output is interleaved.
    sys.stdout.flush()
    sys.stderr.flush()

    # File position pointers are shared across processes, so we must open
    # our own file descriptor to ensure output is not lost.
    output_name = output.name
    with open(output_name, 'r') as output:
      os.unlink(output_name)
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
        buf = output.read(_BUFSIZE)
        while len(buf) > 0:
          if not silent:
            sys.stdout.write(buf)
          pos += len(buf)
          if len(buf) < _BUFSIZE:
            break
          buf = output.read(_BUFSIZE)
        sys.stdout.flush()

    # Propagate any results.
    for result in results:
      results_lib.Results.Record(*result)

    # If a traceback occurred, return it.
    return error

  def Empty(self):
    """Return True if there are any steps left to run."""
    return len(self._steps) == 0

  def start(self):
    """Invoke multiprocessing.Process.start after flushing output/err."""
    sys.stdout.flush()
    sys.stderr.flush()
    return multiprocessing.Process.start(self)

  def run(self):
    """Run the list of steps."""
    if self._semaphore is not None:
      self._semaphore.acquire()
    try:
      self._RunSteps()
    finally:
      if self._semaphore is not None:
        self._semaphore.release()

  def _RunSteps(self):
    """Internal method for running the list of steps."""

    sys.stdout.flush()
    sys.stderr.flush()
    orig_stdout, orig_stderr = sys.stdout, sys.stderr
    stdout_fileno = sys.__stdout__.fileno()
    stderr_fileno = sys.__stderr__.fileno()
    orig_stdout_fd, orig_stderr_fd = map(os.dup,
                                         [stdout_fileno, stderr_fileno])
    while self._steps:
      step, output = self._steps.popleft()
      # Send all output to a named temporary file.
      os.dup2(output.fileno(), stdout_fileno)
      os.dup2(output.fileno(), stderr_fileno)
      # Replace std[out|err] with unbuffered file objects
      sys.stdout = os.fdopen(sys.__stdout__.fileno(), 'w', 0)
      sys.stderr = os.fdopen(sys.__stderr__.fileno(), 'w', 0)
      error = None
      try:
        results_lib.Results.Clear()
        step()
      except results_lib.StepFailure as ex:
        error = str(ex)
      except BaseException as ex:
        error = traceback.format_exc()
        # If it's a fatal exception, don't run any more steps.
        if isinstance(ex, (SystemExit, KeyboardInterrupt)):
          self._steps = []

      sys.stdout.flush()
      sys.stderr.flush()
      output.close()
      sys.stdout, sys.stderr = orig_stdout, orig_stderr
      os.dup2(orig_stdout_fd, stdout_fileno)
      os.dup2(orig_stderr_fd, stderr_fileno)
      map(os.close, [orig_stdout_fd, orig_stderr_fd])
      results = results_lib.Results.Get()
      self._queue.put((error, results))


@contextlib.contextmanager
def _ParallelSteps(steps, max_parallel=None, hide_output_after_errors=False):
  """Run a list of functions in parallel.

  This function launches the provided functions in the background, yields,
  and then waits for the functions to exit.

  The output from the functions is saved to a temporary file and printed as if
  they were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel tasks have finished running. Further, a
  BackgroundFailure is raised with full stack traces of all exceptions.

  Args:
    steps: A list of functions to run.
    max_parallel: The maximum number of simultaneous tasks to run in parallel.
      By default, run all tasks in parallel.
    hide_output_after_errors: After the first exception occurs, squelch any
      further output, including any exceptions that might occur.
  """

  semaphore = None
  if max_parallel is not None:
    semaphore = multiprocessing.Semaphore(max_parallel)

  # First, start all the steps.
  bg_steps = []
  for step in steps:
    bg = _BackgroundSteps(semaphore)
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
        silent = tracebacks and hide_output_after_errors
        error = bg.WaitForStep(silent=silent)
        if not silent and error is not None:
          tracebacks.append(error)
      bg.join()

    # Propagate any exceptions.
    if tracebacks:
      raise BackgroundFailure('\n' + ''.join(tracebacks))


def RunParallelSteps(steps, max_parallel=None, hide_output_after_errors=False):
  """Run a list of functions in parallel.

  This function blocks until all steps are completed.

  The output from the functions is saved to a temporary file and printed as if
  they were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel tasks have finished running. Further, a
  BackgroundFailure is raised with full stack traces of all exceptions.

  Args:
    steps: A list of functions to run.
    max_parallel: The maximum number of simultaneous tasks to run in parallel.
      By default, run all tasks in parallel.
    hide_output_after_errors: After the first exception occurs, squelch any
      further output, including any exceptions that might occur.

  Example:
    # This snippet will execute in parallel:
    #   somefunc()
    #   anotherfunc()
    #   funcfunc()
    steps = [somefunc, anotherfunc, funcfunc]
    RunParallelSteps(steps)
    # Blocks until all calls have completed.
  """
  with _ParallelSteps(steps, max_parallel=max_parallel,
                      hide_output_after_errors=hide_output_after_errors):
    pass


class _AllTasksComplete(object):
  """Sentinel object to indicate that all tasks are complete."""


def _TaskRunner(queue, task, onexit=None):
  """Run task(*input) for each input in the queue.

  Returns when it encounters an _AllTasksComplete object on the queue.
  If exceptions occur, save them off and re-raise them as a
  BackgroundFailure once we've finished processing the items in the queue.

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
      break

    # If no tasks failed yet, process the remaining tasks.
    if not tracebacks:
      try:
        task(*x)
      except BaseException:
        tracebacks.append(traceback.format_exc())

  # Run exit handlers.
  if onexit:
    onexit()

  # Propagate any exceptions.
  if tracebacks:
    raise BackgroundFailure('\n' + ''.join(tracebacks))


@contextlib.contextmanager
def BackgroundTaskRunner(task, queue=None, processes=None, onexit=None):
  """Run the specified task on each queued input in a pool of processes.

  This context manager starts a set of workers in the background, who each
  wait for input on the specified queue. These workers run task(*input) for
  each input on the queue.

  The output from these tasks is saved to a temporary file. When control
  returns to the context manager, the background output is printed in order,
  as if the tasks were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel tasks have finished running. Further, a
  BackgroundFailure is raised with full stack traces of all exceptions.

  Example:
    # This will run somefunc('small', 'cow') in the background
    # while "more random stuff" is being executed.

    def somefunc(arg1, arg2):
      ...
    ...
    with BackgroundTaskRunner(somefunc) as queue:
      ... do random stuff ...
      queue.put(['small', 'cow'])
      ... do more random stuff ...
    # Exiting the with statement will block until all calls have completed.

  Args:
    task: Function to run on each queued input.
    queue: A queue of tasks to run. Add tasks to this queue, and they will
      be run in the background.  If None, one will be created on the fly.
    processes: Number of processes to launch.
    onexit: Function to run in each background process after all inputs are
      processed.
  """

  if queue is None:
    queue = multiprocessing.Queue()

  if not processes:
    processes = multiprocessing.cpu_count()

  steps = [functools.partial(_TaskRunner, queue, task, onexit)] * processes
  with _ParallelSteps(steps):
    try:
      yield queue
    finally:
      for _ in xrange(processes):
        queue.put(_AllTasksComplete())


def RunTasksInProcessPool(task, inputs, processes=None, onexit=None):
  """Run the specified function with each supplied input in a pool of processes.

  This function runs task(*x) for x in inputs in a pool of processes. This
  function blocks until all tasks are completed.

  The output from these tasks is saved to a temporary file. When control
  returns to the context manager, the background output is printed in order,
  as if the tasks were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel tasks have finished running. Further, a
  BackgroundFailure is raised with full stack traces of all exceptions.

  Example:
    # This snippet will execute in parallel:
    #   somefunc('hi', 'fat', 'code')
    #   somefunc('foo', 'bar', 'cow')

    def somefunc(arg1, arg2, arg3):
      ...
    ...
    inputs = [
      ['hi', 'fat', 'code'],
      ['foo', 'bar', 'cow'],
    ]
    RunTasksInProcessPool(somefunc, inputs)
    # Blocks until all calls have completed.

  Args:
    task: Function to run on each input.
    inputs: List of inputs.
    processes: Number of processes, at most, to launch.
    onexit: Function to run in each background process after all inputs are
      processed.
  """

  if not processes:
    processes = min(multiprocessing.cpu_count(), len(inputs))

  with BackgroundTaskRunner(task, processes=processes, onexit=onexit) as queue:
    for x in inputs:
      queue.put(x)
