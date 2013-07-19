#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import logging
import multiprocessing
import os
import signal
import sys
import tempfile
import time
import Queue

sys.path.insert(0, os.path.abspath('%s/../../..' % __file__))
from chromite.lib import cros_test_lib
from chromite.lib import parallel
from chromite.lib import partial_mock

# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock

# pylint: disable=W0212
_BUFSIZE = 10**4
_NUM_WRITES = 100
_NUM_THREADS = 50
_TOTAL_BYTES = _NUM_THREADS * _NUM_WRITES * _BUFSIZE
_GREETING = 'hello world'


class ParallelMock(partial_mock.PartialMock):
  """Run parallel steps in sequence for testing purposes.

  This class updates chromite.lib.parallel to just run processes in
  sequence instead of running them in parallel. This is useful for
  testing.
  """

  TARGET = 'chromite.lib.parallel._BackgroundTask'
  ATTRS = ('ParallelTasks', 'TaskRunner')

  @contextlib.contextmanager
  def ParallelTasks(self, steps, max_parallel=None, halt_on_error=False):
    assert max_parallel is None or isinstance(max_parallel, (int, long))
    assert isinstance(halt_on_error, bool)
    try:
      yield
    finally:
      for step in steps:
        step()

  def TaskRunner(self, queue, task, onexit=None, task_args=None,
                 task_kwargs=None):
    # Setup of these matches the original code.
    if task_args is None:
      task_args = []
    elif not isinstance(task_args, list):
      task_args = list(task_args)
    if task_kwargs is None:
      task_kwargs = {}

    try:
      while True:
        # Wait for a new item to show up on the queue. This is a blocking wait,
        # so if there's nothing to do, we just sit here.
        x = queue.get()
        if isinstance(x, parallel._AllTasksComplete):
          # All tasks are complete, so we should exit.
          break
        x = task_args + list(x)
        task(*x, **task_kwargs)
    finally:
      if onexit:
        onexit()


class BackgroundTaskVerifier(partial_mock.PartialMock):
  """Verify that queues are empty after BackgroundTaskRunner runs.

  BackgroundTaskRunner should always empty its input queues, even if an
  exception occurs. This is important for preventing a deadlock in the case
  where a thread fails partway through (e.g. user presses Ctrl-C before all
  input can be processed).
  """

  TARGET = 'chromite.lib.parallel'
  ATTRS = ('BackgroundTaskRunner',)

  @contextlib.contextmanager
  def BackgroundTaskRunner(self, task, *args, **kwargs):
    queue = kwargs.setdefault('queue', multiprocessing.Queue())
    args = [task] + list(args)
    try:
      with self.backup['BackgroundTaskRunner'](*args, **kwargs):
        yield queue
    finally:
      try:
        queue.get(False)
      except Queue.Empty:
        pass
      else:
        raise AssertionError('Expected empty queue after BackgroundTaskRunner')


class TestBackgroundWrapper(cros_test_lib.TestCase):

  def setUp(self):
    self.tempfile = None

  def tearDown(self):
    # Complain if there are any children left over.
    active_children = multiprocessing.active_children()
    for child in active_children:
      child.Kill(signal.SIGKILL, log_level=logging.WARNING)
      child.join()
    self.assertEqual(multiprocessing.active_children(), [])
    self.assertEqual(active_children, [])

  def wrapOutputTest(self, func):
    # Set _PRINT_INTERVAL to a smaller number to make it easier to
    # reproduce bugs.
    with mock.patch.multiple(parallel._BackgroundTask, PRINT_INTERVAL=0.01):
      with tempfile.NamedTemporaryFile(bufsize=0) as output:
        with mock.patch.multiple(sys, stdout=output):
          func()
        with open(output.name, 'r', 0) as tmp:
          tmp.seek(0)
          return tmp.read()


class TestHelloWorld(TestBackgroundWrapper):

  def setUp(self):
    self.printed_hello = multiprocessing.Event()

  def _HelloWorld(self):
    """Write 'hello world' to stdout."""
    sys.stdout.write('hello')
    sys.stdout.flush()
    sys.stdout.seek(0)
    self.printed_hello.set()

    # Wait for the parent process to read the output. Once the output
    # has been read, try writing 'hello world' again, to be sure that
    # rewritten output is not read twice.
    time.sleep(parallel._BackgroundTask.PRINT_INTERVAL * 10)
    sys.stdout.write(_GREETING)
    sys.stdout.flush()

  def _ParallelHelloWorld(self):
    """Write 'hello world' to stdout using multiple processes."""
    queue = multiprocessing.Queue()
    with parallel.BackgroundTaskRunner(self._HelloWorld, queue=queue):
      queue.put([])
      self.printed_hello.wait()

  def VerifyDefaultQueue(self):
    """Verify that BackgroundTaskRunner will create a queue on it's own."""
    with parallel.BackgroundTaskRunner(self._HelloWorld) as queue:
      queue.put([])
      self.printed_hello.wait()

  def testParallelHelloWorld(self):
    """Test that output is not written multiple times when seeking."""
    out = self.wrapOutputTest(self._ParallelHelloWorld)
    self.assertEquals(out, _GREETING)

  def testMultipleHelloWorlds(self):
    """Test that multiple threads can be created."""
    parallel.RunParallelSteps([self.testParallelHelloWorld] * 2)


def _BackgroundTaskRunnerArgs(results, arg1, arg2, kwarg1=None, kwarg2=None):
  """Helper for TestBackgroundTaskRunnerArgs

  We specifically want a module function to test against and not a class member.
  """
  results.put((arg1, arg2, kwarg1, kwarg2))


class TestBackgroundTaskRunnerArgs(TestBackgroundWrapper):

  def testArgs(self):
    """Test that we can pass args down to the task."""
    results = multiprocessing.Queue()
    arg2s = set((1, 2, 3))
    with parallel.BackgroundTaskRunner(_BackgroundTaskRunnerArgs, results,
                                       'arg1', kwarg1='kwarg1') as queue:
      for arg2 in arg2s:
        queue.put((arg2,))

    # Since the queue is unordered, need to handle arg2 specially.
    result_arg2s = set()
    for _ in xrange(3):
      result = results.get()
      self.assertEquals(result[0], 'arg1')
      result_arg2s.add(result[1])
      self.assertEquals(result[2], 'kwarg1')
      self.assertEquals(result[3], None)
    self.assertEquals(arg2s, result_arg2s)
    self.assertEquals(results.empty(), True)


class TestRunParallelSteps(cros_test_lib.TestCase):
  """Tests for RunParallelSteps."""

  def testReturnValues(self):
    """Test that we pass return values through when requested."""
    def f1():
      return 1
    def f2():
      return 2
    def f3():
      pass

    return_values = parallel.RunParallelSteps([f1, f2, f3], return_values=True)
    self.assertEquals(return_values, [1, 2, None])

  def testLargeReturnValues(self):
    """Test that the managed queue prevents hanging on large return values."""
    def f1():
      return ret_value

    ret_value = ""
    for _ in xrange(10000):
      ret_value += 'This will be repeated many times.\n'

    return_values = parallel.RunParallelSteps([f1], return_values=True)
    self.assertEquals(return_values, [ret_value])


class TestParallelMock(TestBackgroundWrapper):
  """Test the ParallelMock class."""

  def setUp(self):
    self._calls = 0

  def _Callback(self):
    self._calls += 1

  def testRunParallelSteps(self):
    """Make sure RunParallelSteps is mocked out."""
    with ParallelMock():
      parallel.RunParallelSteps([self._Callback])
      self.assertEqual(1, self._calls)

  def testBackgroundTaskRunner(self):
    """Make sure BackgroundTaskRunner is mocked out."""
    with ParallelMock():
      parallel.RunTasksInProcessPool(self._Callback, [])
      self.assertEqual(0, self._calls)
      parallel.RunTasksInProcessPool(self._Callback, [[]])
      self.assertEqual(1, self._calls)
      parallel.RunTasksInProcessPool(self._Callback, [], processes=9,
                                     onexit=self._Callback)
      self.assertEqual(10, self._calls)


class TestExceptions(cros_test_lib.MockOutputTestCase):
  """Test cases where child processes raise exceptions."""

  def _SystemExit(self):
    sys.stdout.write(_GREETING)
    sys.exit(1)

  def _KeyboardInterrupt(self):
    sys.stdout.write(_GREETING)
    raise KeyboardInterrupt()

  def testExceptionRaising(self):
    # pylint: disable=E1101
    self.StartPatcher(BackgroundTaskVerifier())
    for fn in (self._SystemExit, self._KeyboardInterrupt):
      for task in (lambda: parallel.RunTasksInProcessPool(fn, [[]]),
                   lambda: parallel.RunParallelSteps([fn])):
        output_str = ex_str = None
        with self.OutputCapturer() as capture:
          try:
            task()
          except parallel.BackgroundFailure as ex:
            output_str = capture.GetStdout()
            ex_str = str(ex)
        self.assertTrue('Traceback' in ex_str)
        self.assertEqual(output_str, _GREETING)


if __name__ == '__main__':
  # Set timeouts small so that if the unit test hangs, it won't hang for long.
  parallel._BackgroundTask.STARTUP_TIMEOUT = 5
  parallel._BackgroundTask.EXIT_TIMEOUT = 5

  # Run the tests.
  cros_test_lib.main(level=logging.INFO)
