# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

# pylint: disable=F0401
import mojo.embedder
from mojo import system


def _Increment(array):
  def _Closure():
    array.append(0)
  return _Closure


class RunLoopTest(unittest.TestCase):

  def setUp(self):
    mojo.embedder.Init()

  def testRunLoop(self):
    loop = system.RunLoop()
    array = []
    for _ in xrange(10):
      loop.PostDelayedTask(_Increment(array))
    loop.RunUntilIdle()
    self.assertEquals(len(array), 10)

  def testRunLoopWithException(self):
    loop = system.RunLoop()
    def Throw():
      raise Exception("error")
    array = []
    loop.PostDelayedTask(Throw)
    loop.PostDelayedTask(_Increment(array))
    with self.assertRaisesRegexp(Exception, '^error$'):
      loop.Run()
    self.assertEquals(len(array), 0)
    loop.RunUntilIdle()
    self.assertEquals(len(array), 1)
