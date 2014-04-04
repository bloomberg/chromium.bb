# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for running cbuildbot stages in the background."""

import collections
import contextlib
import errno
import functools
import logging
import multiprocessing
import os
try:
  import Queue
except ImportError:
  # Python-3 renamed to "queue".  We still use Queue to avoid collisions
  # with naming variables as "queue".  Maybe we'll transition at some point.
  # pylint: disable=F0401
  import queue as Queue
import signal
import sys
import tempfile
import time
import traceback

from chromite.buildbot import cbuildbot_results as results_lib
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import timeout_util

_BUFSIZE = 1024

logger = logging.getLogger(__name__)


def Manager():
  """Create a background process for managing interprocess communication.

  This manager wraps multiprocessing.Manager() and ensures that any sockets
  created during initialization are created in '/tmp' rather than in a custom
  temp directory. This is needed because TMPDIR might be really long, and
  named sockets are limited to 108 characters.

  Usage:
    with Manager() as manager:
      queue = manager.Queue()
      ...

  Returns:
    The return value of multiprocessing.Manager()
  """
  old_tempdir_value, old_tempdir_env = osutils.SetGlobalTempDir('/tmp')
  try:
    return multiprocessing.Manager()
  finally:
    osutils.SetGlobalTempDir(old_tempdir_value, old_tempdir_env)


class BackgroundFailure(results_lib.StepFailure):
  """Exception to show a step failed while running in a background process."""


class _BackgroundTask(multiprocessing.Process):
  """Run a task in the background.

  This task may be the 'Run' function from a buildbot stage or just a plain
  function. It will be run in the background. Output from this task is saved
  to a temporary file and is printed when the 'Wait' function is called.
  """

  # The time we give Python to startup and exit.
  STARTUP_TIMEOUT = 60 * 5
  EXIT_TIMEOUT = 60 * 10

  # TODO: crbug.com/360063
  # The time we allow processes to be silent.
  SILENT_TIMEOUT = 60 * 205

  # The amount by which we reduce the SILENT_TIMEOUT every time we launch
  # a subprocess. This helps ensure that children get a chance to enforce the
  # SILENT_TIMEOUT prior to the parents enforcing it.
  SILENT_TIMEOUT_STEP = 30
  MINIMUM_SILENT_TIMEOUT = 60 * 135

  # The time before terminating or killing a task.
  SIGTERM_TIMEOUT = 30
  SIGKILL_TIMEOUT = 60

  # Interval we check for updates from print statements.
  PRINT_INTERVAL = 1

  def __init__(self, task, queue, semaphore=None, task_args=None,
               task_kwargs=None):
    """Create a new _BackgroundTask object.

    If semaphore is supplied, it will be acquired for the duration of the
    steps that are run in the background. This can be used to limit the
    number of simultaneous parallel tasks.

    Args:
      task: The task (a functor) to run in the background.
      queue: A queue to be used for managing communication between the parent
        and child process. This queue must be valid for the length of the
        life of the child process, until the parent has collected its status.
      semaphore: The lock to hold while |task| runs.
      task_args: A list of args to pass to the |task|.
      task_kwargs: A dict of optional args to pass to the |task|.
    """
    multiprocessing.Process.__init__(self)
    self._task = task
    self._queue = queue
    self._semaphore = semaphore
    self._started = multiprocessing.Event()
    self._killing = multiprocessing.Event()
    self._output = None
    self._parent_pid = None
    self._task_args = task_args if task_args else ()
    self._task_kwargs = task_kwargs if task_kwargs else {}

  def _WaitForStartup(self):
    # TODO(davidjames): Use python-2.7 syntax to simplify this.
    self._started.wait(self.STARTUP_TIMEOUT)
    msg = 'Process failed to start in %d seconds' % self.STARTUP_TIMEOUT
    assert self._started.is_set(), msg

  def Kill(self, sig, log_level):
    """Kill process with signal, ignoring if the process is dead.

    Args:
      sig: Signal to send.
      log_level: The log level of log messages.
    """
    self._killing.set()
    self._WaitForStartup()
    if logger.isEnabledFor(log_level):
      # Print a pstree of the hanging process.
      logger.log(log_level, 'Killing %r (sig=%r)', self.pid, sig)
      cros_build_lib.RunCommand(['pstree', '-Apal', str(self.pid)],
                                debug_level=log_level, error_code_ok=True,
                                log_output=True)

    try:
      os.kill(self.pid, sig)
    except OSError as ex:
      if ex.errno != errno.ESRCH:
        raise

  def Cleanup(self, silent=False):
    """Wait for a process to exit."""
    if os.getpid() != self._parent_pid or self._output is None:
      return
    try:
      # Print output from subprocess.
      if not silent and logger.isEnabledFor(logging.DEBUG):
        with open(self._output.name, 'r') as f:
          for line in f:
            logging.debug(line.rstrip('\n'))
    finally:
      # Clean up our temporary file.
      osutils.SafeUnlink(self._output.name)
      self._output.close()
      self._output = None

  def Wait(self):
    """Wait for the task to complete.

    Output from the task is printed as it runs.

    If an exception occurs, return a string containing the traceback.
    """
    try:
      # Flush stdout and stderr to be sure no output is interleaved.
      sys.stdout.flush()
      sys.stderr.flush()

      # File position pointers are shared across processes, so we must open
      # our own file descriptor to ensure output is not lost.
      self._WaitForStartup()
      silent_death_time = time.time() + self.SILENT_TIMEOUT
      results = []
      with open(self._output.name, 'r') as output:
        pos = 0
        running, exited_cleanly, msg, error = (True, False, None, None)
        possibly_flaky = False
        while running:
          # Check whether the process is still alive.
          running = self.is_alive()

          try:
            error, results, possibly_flaky = \
                self._queue.get(True, self.PRINT_INTERVAL)
            running = False
            exited_cleanly = True
          except Queue.Empty:
            pass

          if not running:
            # Wait for the process to actually exit. If the child doesn't exit
            # in a timely fashion, kill it.
            self.join(self.EXIT_TIMEOUT)
            if self.exitcode is None:
              msg = '%r hung for %r seconds' % (self, self.EXIT_TIMEOUT)
              self._KillChildren([self])
            elif not exited_cleanly:
              # Treat SIGKILL signals as potentially flaky.
              if self.exitcode == -signal.SIGKILL:
                possibly_flaky = True
              msg = ('%r exited unexpectedly with code %s' %
                     (self, self.exitcode))

          # Read output from process.
          output.seek(pos)
          buf = output.read(_BUFSIZE)

          if len(buf) > 0:
            silent_death_time = time.time() + self.SILENT_TIMEOUT
          elif running and time.time() > silent_death_time:
            msg = ('No output from %r for %r seconds' %
                   (self, self.SILENT_TIMEOUT))
            self._KillChildren([self])

            # Timeouts are possibly flaky.
            possibly_flaky = True

            # Read remaining output from the process.
            output.seek(pos)
            buf = output.read(_BUFSIZE)
            running = False

          # Print output so far.
          while len(buf) > 0:
            sys.stdout.write(buf)
            pos += len(buf)
            if len(buf) < _BUFSIZE:
              break
            buf = output.read(_BUFSIZE)

          # Print error messages if anything exceptional occurred.
          if msg:
            cros_build_lib.PrintBuildbotStepFailure()
            error = '\n'.join(x for x in (error, msg) if x)
            logger.warning(error)
            traceback.print_stack()

          sys.stdout.flush()
          sys.stderr.flush()

      # Propagate any results.
      for result in results:
        results_lib.Results.Record(*result)

    finally:
      self.Cleanup(silent=True)

    # If a traceback occurred, return it.
    return error, possibly_flaky

  def start(self):
    """Invoke multiprocessing.Process.start after flushing output/err."""
    if self.SILENT_TIMEOUT < self.MINIMUM_SILENT_TIMEOUT:
      raise AssertionError('Maximum recursion depth exceeded in %r' % self)

    sys.stdout.flush()
    sys.stderr.flush()
    self._output = tempfile.NamedTemporaryFile(delete=False, bufsize=0,
                                               prefix='chromite-parallel-')
    self._parent_pid = os.getpid()
    return multiprocessing.Process.start(self)

  def run(self):
    """Run the list of steps."""
    if self._semaphore is not None:
      self._semaphore.acquire()

    error = 'Unexpected exception in %r' % self
    possibly_flaky = False
    pid = os.getpid()
    try:
      error, possibly_flaky = self._Run()
    finally:
      if not self._killing.is_set() and os.getpid() == pid:
        results = results_lib.Results.Get()
        self._queue.put((error, results, possibly_flaky))
        if self._semaphore is not None:
          self._semaphore.release()

  def _Run(self):
    """Internal method for running the list of steps."""

    # The default handler for SIGINT sometimes forgets to actually raise the
    # exception (and we can reproduce this using unit tests), so we define a
    # custom one instead.
    def kill_us(_sig_num, _frame):
      raise KeyboardInterrupt('SIGINT received')
    signal.signal(signal.SIGINT, kill_us)

    sys.stdout.flush()
    sys.stderr.flush()
    possibly_flaky = False
    # Send all output to a named temporary file.
    with open(self._output.name, 'w', 0) as output:
      # Back up sys.std{err,out}. These aren't used, but we keep a copy so
      # that they aren't garbage collected. We intentionally don't restore
      # the old stdout and stderr at the end, because we want shutdown errors
      # to also be sent to the same log file.
      _orig_stdout, _orig_stderr = sys.stdout, sys.stderr

      # Replace std{out,err} with unbuffered file objects.
      os.dup2(output.fileno(), sys.__stdout__.fileno())
      os.dup2(output.fileno(), sys.__stderr__.fileno())
      sys.stdout = os.fdopen(sys.__stdout__.fileno(), 'w', 0)
      sys.stderr = os.fdopen(sys.__stderr__.fileno(), 'w', 0)

      error = None
      try:
        self._started.set()
        results_lib.Results.Clear()

        # Reduce the silent timeout by the prescribed amount.
        cls = self.__class__
        cls.SILENT_TIMEOUT -= cls.SILENT_TIMEOUT_STEP

        # Actually launch the task.
        self._task(*self._task_args, **self._task_kwargs)
      except results_lib.StepFailure as ex:
        error = str(ex)
        possibly_flaky = ex.possibly_flaky
      except BaseException as ex:
        possibly_flaky = isinstance(ex, timeout_util.TimeoutError)
        error = traceback.format_exc()
        if self._killing.is_set():
          traceback.print_exc()
      finally:
        sys.stdout.flush()
        sys.stderr.flush()

    return error, possibly_flaky

  @classmethod
  def _KillChildren(cls, bg_tasks, log_level=logging.WARNING):
    """Kill a deque of background tasks.

    This is needed to prevent hangs in the case where child processes refuse
    to exit.

    Args:
      bg_tasks: A list filled with _BackgroundTask objects.
      log_level: The log level of log messages.
    """
    logger.log(log_level, 'Killing tasks: %r', bg_tasks)
    signals = ((signal.SIGINT, cls.SIGTERM_TIMEOUT),
               (signal.SIGTERM, cls.SIGKILL_TIMEOUT),
               (signal.SIGKILL, None))
    for sig, timeout in signals:
      # Send signal to all tasks.
      for task in bg_tasks:
        task.Kill(sig, log_level)

      # Wait for all tasks to exit, if requested.
      if timeout is None:
        for task in bg_tasks:
          task.join()
          task.Cleanup()
        break

      # Wait until timeout expires.
      end_time = time.time() + timeout
      while bg_tasks:
        time_left = end_time - time.time()
        if time_left <= 0:
          break
        task = bg_tasks[-1]
        task.join(time_left)
        if task.exitcode is not None:
          task.Cleanup()
          bg_tasks.pop()

  @classmethod
  @contextlib.contextmanager
  def ParallelTasks(cls, steps, max_parallel=None, halt_on_error=False):
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
      halt_on_error: After the first exception occurs, halt any running steps,
        and squelch any further output, including any exceptions that might
        occur.
    """

    semaphore = None
    possibly_flaky = False
    if max_parallel is not None:
      semaphore = multiprocessing.Semaphore(max_parallel)

    # First, start all the steps.
    with Manager() as manager:
      bg_tasks = collections.deque()
      for step in steps:
        task = cls(step, queue=manager.Queue(), semaphore=semaphore)
        task.start()
        bg_tasks.append(task)

      try:
        yield
      finally:
        # Wait for each step to complete.
        tracebacks = []
        flaky_tasks = []
        while bg_tasks:
          task = bg_tasks.popleft()
          error, possibly_flaky = task.Wait()
          if error is not None:
            flaky_tasks.append(possibly_flaky)
            tracebacks.append(error)
            if halt_on_error:
              break

        # If there are still tasks left, kill them.
        if bg_tasks:
          cls._KillChildren(bg_tasks, log_level=logging.DEBUG)

        # Propagate any exceptions.
        if tracebacks:
          possibly_flaky = flaky_tasks and all(flaky_tasks)
          raise BackgroundFailure('\n' + ''.join(tracebacks), possibly_flaky)

  @staticmethod
  def TaskRunner(queue, task, onexit=None, task_args=None, task_kwargs=None):
    """Run task(*input) for each input in the queue.

    Returns when it encounters an _AllTasksComplete object on the queue.
    If exceptions occur, save them off and re-raise them as a
    BackgroundFailure once we've finished processing the items in the queue.

    Args:
      queue: A queue of tasks to run. Add tasks to this queue, and they will
        be run.
      task: Function to run on each queued input.
      onexit: Function to run after all inputs are processed.
      task_args: A list of args to pass to the |task|.
      task_kwargs: A dict of optional args to pass to the |task|.
    """
    if task_args is None:
      task_args = []
    elif not isinstance(task_args, list):
      task_args = list(task_args)
    if task_kwargs is None:
      task_kwargs = {}

    tracebacks = []
    while True:
      # Wait for a new item to show up on the queue. This is a blocking wait,
      # so if there's nothing to do, we just sit here.
      x = queue.get()
      if isinstance(x, _AllTasksComplete):
        # All tasks are complete, so we should exit.
        break
      elif not isinstance(x, list):
        x = task_args + list(x)
      else:
        x = task_args + x

      # If no tasks failed yet, process the remaining tasks.
      if not tracebacks:
        try:
          task(*x, **task_kwargs)
        except BaseException:
          tracebacks.append(traceback.format_exc())

    # Run exit handlers.
    if onexit:
      onexit()

    # Propagate any exceptions.
    if tracebacks:
      raise BackgroundFailure('\n' + ''.join(tracebacks))


def RunParallelSteps(steps, max_parallel=None, halt_on_error=False,
                     return_values=False):
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
    halt_on_error: After the first exception occurs, halt any running steps,
      and squelch any further output, including any exceptions that might occur.
    return_values: If set to True, RunParallelSteps returns a list containing
      the return values of the steps.  Defaults to False.

  Returns:
    If |return_values| is True, the function will return a list containing the
    return values of the steps.

  Example:
    # This snippet will execute in parallel:
    #   somefunc()
    #   anotherfunc()
    #   funcfunc()
    steps = [somefunc, anotherfunc, funcfunc]
    RunParallelSteps(steps)
    # Blocks until all calls have completed.
  """
  def ReturnWrapper(queue, fn):
    """Put the return value of |fn| into |queue|."""
    queue.put(fn())

  full_steps = []
  queues = []
  with cros_build_lib.ContextManagerStack() as stack:
    if return_values:
      # We use a managed queue here, because the child process will wait for the
      # queue(pipe) to be flushed (i.e., when items are read from the queue)
      # before exiting, and with a regular queue this may result in hangs for
      # large return values.  But with a managed queue, the manager process will
      # read the items and hold on to them until the managed queue goes out of
      # scope and is cleaned up.
      manager = stack.Add(Manager)
      for step in steps:
        queue = manager.Queue()
        queues.append(queue)
        full_steps.append(functools.partial(ReturnWrapper, queue, step))
    else:
      full_steps = steps

    with _BackgroundTask.ParallelTasks(full_steps, max_parallel=max_parallel,
                                       halt_on_error=halt_on_error):
      pass

    if return_values:
      return [queue.get_nowait() for queue in queues]


class _AllTasksComplete(object):
  """Sentinel object to indicate that all tasks are complete."""


@contextlib.contextmanager
def BackgroundTaskRunner(task, *args, **kwargs):
  """Run the specified task on each queued input in a pool of processes.

  This context manager starts a set of workers in the background, who each
  wait for input on the specified queue. For each input on the queue, these
  workers run task(*args + *input, **kwargs). Note that certain kwargs will
  not pass through to the task (see Args below for the list).

  The output from these tasks is saved to a temporary file. When control
  returns to the context manager, the background output is printed in order,
  as if the tasks were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel tasks have finished running. Further, a
  BackgroundFailure is raised with full stack traces of all exceptions.

  Example:
    # This will run somefunc(1, 'small', 'cow', foo='bar') in the background
    # as soon as data is added to the queue (i.e. queue.put() is called).

    def somefunc(arg1, arg2, arg3, foo=None):
      ...

    with BackgroundTaskRunner(somefunc, 1, foo='bar') as queue:
      ... do random stuff ...
      queue.put(['small', 'cow'])
      ... do more random stuff while somefunc() runs ...
    # Exiting the with statement will block until all calls have completed.

  Args:
    task: Function to run on each queued input.
    queue: A queue of tasks to run. Add tasks to this queue, and they will
      be run in the background.  If None, one will be created on the fly.
    processes: Number of processes to launch.
    onexit: Function to run in each background process after all inputs are
      processed.
  """

  queue = kwargs.pop('queue', None)
  processes = kwargs.pop('processes', None)
  onexit = kwargs.pop('onexit', None)

  with cros_build_lib.ContextManagerStack() as stack:
    if queue is None:
      manager = stack.Add(Manager)
      queue = manager.Queue()

    if not processes:
      processes = multiprocessing.cpu_count()

    child = functools.partial(_BackgroundTask.TaskRunner, queue, task,
                              onexit=onexit, task_args=args,
                              task_kwargs=kwargs)
    steps = [child] * processes
    with _BackgroundTask.ParallelTasks(steps):
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
    # - Use >=16 processes by default, in case it's a network-bound operation.
    # - Try to use all of the CPUs, in case it's a CPU-bound operation.
    processes = min(max(16, multiprocessing.cpu_count()), len(inputs))

  with BackgroundTaskRunner(task, processes=processes, onexit=onexit) as queue:
    for x in inputs:
      queue.put(x)
