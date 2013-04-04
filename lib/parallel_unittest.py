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
from chromite.lib import osutils
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

  def TaskRunner(self, queue, task, onexit=None):
    try:
      while True:
        # Wait for a new item to show up on the queue. This is a blocking wait,
        # so if there's nothing to do, we just sit here.
        x = queue.get()
        if isinstance(x, parallel._AllTasksComplete):
          # All tasks are complete, so we should exit.
          break
        task(*x)
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
  def BackgroundTaskRunner(self, task, queue=None, processes=None, onexit=None):
    if queue is None:
      queue = multiprocessing.Queue()
    try:
      with self.backup['BackgroundTaskRunner'](task, queue, processes, onexit):
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


class TestFastPrinting(TestBackgroundWrapper):

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

  def testTempFileCleanup(self):
    """Test that all temp files are cleaned up."""
    with osutils.TempDir() as tempdir:
      self.assertEqual(os.listdir(tempdir), [])
      self.testExceptionRaising()
      self.assertEqual(os.listdir(tempdir), [])


if __name__ == '__main__':
  # Set timeouts small so that if the unit test hangs, it won't hang for long.
  parallel._BackgroundTask.STARTUP_TIMEOUT = 5
  parallel._BackgroundTask.EXIT_TIMEOUT = 5

  # Run the tests.
  cros_test_lib.main(level=logging.INFO)
