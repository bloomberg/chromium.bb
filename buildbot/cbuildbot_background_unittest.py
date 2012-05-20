#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import multiprocessing
import sys
import tempfile
import time
import unittest

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.buildbot import cbuildbot_background as background

# pylint: disable=W0212
_BUFSIZE = 10**4
_NUM_WRITES = 100
_NUM_THREADS = 50
_TOTAL_BYTES = _NUM_THREADS * _NUM_WRITES * _BUFSIZE
_GREETING = 'hello world'


class TestBackgroundWrapper(unittest.TestCase):

  def setUp(self):
    self.tempfile = None

  def wrapOutputTest(self, func):
    # Set _PRINT_INTERVAL to a smaller number to make it easier to
    # reproduce bugs.
    old_print_interval = background._PRINT_INTERVAL
    background._PRINT_INTERVAL = 0.01
    with tempfile.NamedTemporaryFile(bufsize=0) as output:
      old_stdout = sys.stdout
      with open(output.name, 'r', 0) as tmp:
        self.tempfile = tmp
        try:
          sys.stdout = output
          func()
        finally:
          background._PRINT_INTERVAL = old_print_interval
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
    time.sleep(background._PRINT_INTERVAL * 10)
    sys.stdout.write(_GREETING)
    sys.stdout.flush()

  def _ParallelHelloWorld(self):
    """Write 'hello world' to stdout using multiple processes."""
    queue = multiprocessing.Queue()
    with background.BackgroundTaskRunner(queue, self._HelloWorld):
      queue.put([])
      self.printed_hello.wait()

  def testParallelHelloWorld(self):
    """Test that output is not written multiple times when seeking."""
    out = self.wrapOutputTest(self._ParallelHelloWorld)
    self.assertEquals(out, _GREETING)


class TestFastPrinting(TestBackgroundWrapper):

  def _FastPrinter(self):
    # Writing lots of output quickly often reproduces bugs in
    # cbuildbot_background because it can trigger race conditions.
    for _ in range(_NUM_WRITES - 1):
      sys.stdout.write('x' * _BUFSIZE)
    sys.stdout.write('x' * (_BUFSIZE - 1) + '\n')

  def _ParallelPrinter(self):
    background.RunParallelSteps([self._FastPrinter] * _NUM_THREADS)

  def _NestedParallelPrinter(self):
    background.RunParallelSteps([self._ParallelPrinter])

  def testNestedParallelPrinter(self):
    """Verify that no output is lost when lots of output is written."""
    out = self.wrapOutputTest(self._NestedParallelPrinter)
    self.assertEquals(len(out), _TOTAL_BYTES)


if __name__ == '__main__':
  unittest.main()
