#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for parallel library."""

import contextlib
import cPickle
import logging
import multiprocessing
import os
import signal
import sys
import tempfile
import time
import unittest
try:
  import Queue
except ImportError:
  # Python-3 renamed to "queue".  We still use Queue to avoid collisions
  # with naming variables as "queue".  Maybe we'll transition at some point.
  # pylint: disable=F0401
  import queue as Queue

sys.path.insert(0, os.path.abspath('%s/../../..' % __file__))
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import partial_mock
from chromite.lib import timeout_util

# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock

# pylint: disable=W0212
_BUFSIZE = 10**4
_EXIT_TIMEOUT = 30
_NUM_WRITES = 100
_NUM_THREADS = 50
_TOTAL_BYTES = _NUM_THREADS * _NUM_WRITES * _BUFSIZE
_GREETING = 'hello world'
_SKIP_FLAKY_TESTS = True


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
  """Unittests for background wrapper."""

  def setUp(self):
    self.tempfile = None

  def tearDown(self):
    # Wait for children to exit.
    try:
      timeout_util.WaitForReturnValue([[]], multiprocessing.active_children,
                                      timeout=_EXIT_TIMEOUT)
    except timeout_util.TimeoutError:
      pass

    # Complain if there are any children left over.
    active_children = multiprocessing.active_children()
    for child in active_children:
      if hasattr(child, 'Kill'):
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
  """Test HelloWorld output in various background environments."""

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
    with parallel.Manager() as manager:
      queue = manager.Queue()
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

  def testLongTempDirectory(self):
    """Test that we can handle a long temporary directory."""
    with osutils.TempDir() as tempdir:
      new_tempdir = os.path.join(tempdir, 'xxx/' * 100)
      osutils.SafeMakedirs(new_tempdir)
      old_tempdir, old_tempdir_env = osutils.SetGlobalTempDir(new_tempdir)
      try:
        self.testParallelHelloWorld()
      finally:
        osutils.SetGlobalTempDir(old_tempdir, old_tempdir_env)


def _BackgroundTaskRunnerArgs(results, arg1, arg2, kwarg1=None, kwarg2=None):
  """Helper for TestBackgroundTaskRunnerArgs

  We specifically want a module function to test against and not a class member.
  """
  results.put((arg1, arg2, kwarg1, kwarg2))


class TestBackgroundTaskRunnerArgs(TestBackgroundWrapper):
  """Unittests for BackgroundTaskRunner argument handling."""

  def testArgs(self):
    """Test that we can pass args down to the task."""
    with parallel.Manager() as manager:
      results = manager.Queue()
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


class TestFastPrinting(TestBackgroundWrapper):
  """Stress tests for background sys.stdout handling."""

  def _FastPrinter(self):
    # Writing lots of output quickly often reproduces bugs in this module
    # because it can trigger race conditions.
    for _ in range(_NUM_WRITES - 1):
      sys.stdout.write('x' * _BUFSIZE)
    sys.stdout.write('x' * (_BUFSIZE - 1) + '\n')

  def _ParallelPrinter(self):
    parallel.RunParallelSteps([self._FastPrinter] * _NUM_THREADS)

  def _NestedParallelPrinter(self):
    parallel.RunParallelSteps([self._ParallelPrinter])

  def testSimpleParallelPrinter(self):
    out = self.wrapOutputTest(self._ParallelPrinter)
    self.assertEquals(len(out), _TOTAL_BYTES)

  def testNestedParallelPrinter(self):
    """Verify that no output is lost when lots of output is written."""
    out = self.wrapOutputTest(self._NestedParallelPrinter)
    self.assertEquals(len(out), _TOTAL_BYTES)


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

  def _BadPickler(self):
    return self._BadPickler

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

  def testFailedPickle(self):
    """PicklingError should be thrown when an argument fails to pickle."""
    with self.assertRaises(cPickle.PicklingError):
      parallel.RunTasksInProcessPool(self._SystemExit, [self._SystemExit])

  def testFailedPickleOnReturn(self):
    """PicklingError should be thrown when a return value fails to pickle."""
    with self.assertRaises(parallel.BackgroundFailure):
      parallel.RunParallelSteps([self._BadPickler], return_values=True)


class TestHalting(cros_test_lib.MockOutputTestCase, TestBackgroundWrapper):
  """Test that child processes are halted when exceptions occur."""

  def setUp(self):
    self.failed = multiprocessing.Event()
    self.passed = multiprocessing.Event()

  def _Pass(self):
    self.passed.set()
    sys.stdout.write(_GREETING)

  def _Exit(self):
    sys.stdout.write(_GREETING)
    self.passed.wait()
    sys.exit(1)

  def _Fail(self):
    self.failed.wait(60)
    self.failed.set()

  def testExceptionRaising(self):
    """Test that exceptions halt all running steps."""
    steps = [self._Exit, self._Fail, self._Pass, self._Fail]
    output_str, ex_str = None, None
    with self.OutputCapturer() as capture:
      try:
        parallel.RunParallelSteps(steps, halt_on_error=True)
      except parallel.BackgroundFailure as ex:
        output_str = capture.GetStdout()
        ex_str = str(ex)
        logging.debug(ex_str)
    self.assertTrue('Traceback' in ex_str)
    self.assertTrue(self.passed.is_set())
    self.assertEqual(output_str, _GREETING)
    self.assertFalse(self.failed.is_set())

  @unittest.skipIf(_SKIP_FLAKY_TESTS, 'Occasionally fails.')
  def testTempFileCleanup(self):
    """Test that all temp files are cleaned up."""
    with osutils.TempDir() as tempdir:
      self.assertEqual(os.listdir(tempdir), [])
      self.testExceptionRaising()
      self.assertEqual(os.listdir(tempdir), [])

  def testKillQuiet(self, steps=None, **kwargs):
    """Test that processes do get killed if they're silent for too long."""
    if steps is None:
      steps = [self._Fail] * 10
    kwargs.setdefault('SILENT_TIMEOUT', 0.1)
    kwargs.setdefault('MINIMUM_SILENT_TIMEOUT', 0.01)
    kwargs.setdefault('SILENT_TIMEOUT_STEP', 0)
    kwargs.setdefault('SIGTERM_TIMEOUT', 0.1)
    kwargs.setdefault('PRINT_INTERVAL', 0.01)

    ex_str = None
    with mock.patch.multiple(parallel._BackgroundTask, **kwargs):
      with self.OutputCapturer() as capture:
        try:
          with cros_test_lib.LoggingCapturer(cros_build_lib.logger.name):
            with cros_test_lib.LoggingCapturer(parallel.logger.name):
              parallel.RunParallelSteps(steps)
        except parallel.BackgroundFailure as ex:
          ex_str = str(ex)
          error_str = capture.GetStderr()
    self.assertTrue('parallel_unittest.py' in error_str)
    self.assertTrue(ex_str)


if __name__ == '__main__':
  # Run the tests.
  cros_test_lib.main(level=logging.INFO)
