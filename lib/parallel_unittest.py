#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import multiprocessing
import os
import sys
import tempfile
import time

sys.path.insert(0, os.path.abspath('%s/../../..' % __file__))
from chromite.lib import cros_test_lib
from chromite.lib import parallel
from chromite.lib import partial_mock

# pylint: disable=W0212
_BUFSIZE = 10**4
_NUM_WRITES = 100
_NUM_THREADS = 50
_TOTAL_BYTES = _NUM_THREADS * _NUM_WRITES * _BUFSIZE
_GREETING = 'hello world'


class ParallelMock(partial_mock.PartialCmdMock):
  """Run parallel steps in sequence for testing purposes.

  This class updates chromite.lib.parallel to just run processes in
  sequence instead of running them in parallel. This is useful for
  testing.
  """

  TARGET = 'chromite.lib.parallel'
  ATTRS = ('_ParallelSteps',)

  @contextlib.contextmanager
  def _ParallelSteps(self, steps, max_parallel=None):
    assert max_parallel is None or isinstance(max_parallel, (int, long))
    try:
      yield
    finally:
      for step in steps:
        step()


class TestBackgroundWrapper(cros_test_lib.TestCase):

  def setUp(self):
    self.tempfile = None

  def wrapOutputTest(self, func):
    # Set _PRINT_INTERVAL to a smaller number to make it easier to
    # reproduce bugs.
    old_print_interval = parallel._PRINT_INTERVAL
    parallel._PRINT_INTERVAL = 0.01
    with tempfile.NamedTemporaryFile(bufsize=0) as output:
      old_stdout = sys.stdout
      with open(output.name, 'r', 0) as tmp:
        self.tempfile = tmp
        try:
          sys.stdout = output
          func()
        finally:
          parallel._PRINT_INTERVAL = old_print_interval
          sys.stdout = old_stdout
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
    time.sleep(parallel._PRINT_INTERVAL * 10)
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

  def testNestedParallelPrinter(self):
    """Verify that no output is lost when lots of output is written."""
    out = self.wrapOutputTest(self._NestedParallelPrinter)
    self.assertEquals(len(out), _TOTAL_BYTES)


class TestParallelMock(cros_test_lib.TestCase):
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


if __name__ == '__main__':
  cros_test_lib.main()
