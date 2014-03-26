#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import traceback
import unittest


from future import Future


class FutureTest(unittest.TestCase):
  def testNoValueOrDelegate(self):
    self.assertRaises(ValueError, Future)

  def testValue(self):
    future = Future(value=42)
    self.assertEqual(42, future.Get())
    self.assertEqual(42, future.Get())

  def testDelegateValue(self):
    called = [False,]
    def callback():
      self.assertFalse(called[0])
      called[0] = True
      return 42
    future = Future(callback=callback)
    self.assertEqual(42, future.Get())
    self.assertEqual(42, future.Get())

  def testErrorThrowingDelegate(self):
    class FunkyException(Exception):
      pass

    # Set up a chain of functions to test the stack trace.
    def qux():
      raise FunkyException()
    def baz():
      return qux()
    def bar():
      return baz()
    def foo():
      return bar()
    chain = [foo, bar, baz, qux]

    called = [False,]
    def callback():
      self.assertFalse(called[0])
      called[0] = True
      return foo()

    fail = self.fail
    assertTrue = self.assertTrue
    def assert_raises_full_stack(future, err):
      try:
        future.Get()
        fail('Did not raise %s' % err)
      except Exception as e:
        assertTrue(isinstance(e, err))
        stack = traceback.format_exc()
        assertTrue(all(stack.find(fn.__name__) != -1 for fn in chain))

    future = Future(callback=callback)
    assert_raises_full_stack(future, FunkyException)
    assert_raises_full_stack(future, FunkyException)


if __name__ == '__main__':
  unittest.main()
