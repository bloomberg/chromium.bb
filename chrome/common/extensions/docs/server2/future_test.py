#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import traceback
import unittest


from future import All, Future, Race
from mock_function import MockFunction


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

  def testAll(self):
    def callback_with_value(value):
      return MockFunction(lambda: value)

    # Test a single value.
    callback = callback_with_value(42)
    future = All((Future(callback=callback),))
    self.assertTrue(*callback.CheckAndReset(0))
    self.assertEqual([42], future.Get())
    self.assertTrue(*callback.CheckAndReset(1))

    # Test multiple callbacks.
    callbacks = (callback_with_value(1),
                 callback_with_value(2),
                 callback_with_value(3))
    future = All(Future(callback=callback) for callback in callbacks)
    for callback in callbacks:
      self.assertTrue(*callback.CheckAndReset(0))
    self.assertEqual([1, 2, 3], future.Get())
    for callback in callbacks:
      self.assertTrue(*callback.CheckAndReset(1))

    # Test throwing an error.
    def throws_error():
      raise ValueError()
    callbacks = (callback_with_value(1),
                 callback_with_value(2),
                 MockFunction(throws_error))
    future = All(Future(callback=callback) for callback in callbacks)
    for callback in callbacks:
      self.assertTrue(*callback.CheckAndReset(0))
    # Can't check that the callbacks were actually run because in theory the
    # Futures can be resolved in any order.
    self.assertRaises(ValueError, future.Get)

  def testRaceSuccess(self):
    callback = MockFunction(lambda: 42)

    # Test a single value.
    race = Race((Future(callback=callback),))
    self.assertTrue(*callback.CheckAndReset(0))
    self.assertEqual(42, race.Get())
    self.assertTrue(*callback.CheckAndReset(1))

    # Test multiple success values. Note that we could test different values
    # and check that the first returned, but this is just an implementation
    # detail of Race. When we have parallel Futures this might not always hold.
    race = Race((Future(callback=callback),
                 Future(callback=callback),
                 Future(callback=callback)))
    self.assertTrue(*callback.CheckAndReset(0))
    self.assertEqual(42, race.Get())
    # Can't assert the actual count here for the same reason as above.
    callback.CheckAndReset(99)

    # Test values with except_pass.
    def throws_error():
      raise ValueError()
    race = Race((Future(callback=callback),
                 Future(callback=throws_error)),
                 except_pass=(ValueError,))
    self.assertTrue(*callback.CheckAndReset(0))
    self.assertEqual(42, race.Get())
    self.assertTrue(*callback.CheckAndReset(1))

  def testRaceErrors(self):
    def throws_error():
      raise ValueError()

    # Test a single error.
    race = Race((Future(callback=throws_error),))
    self.assertRaises(ValueError, race.Get)

    # Test multiple errors. Can't use different error types for the same reason
    # as described in testRaceSuccess.
    race = Race((Future(callback=throws_error),
                 Future(callback=throws_error),
                 Future(callback=throws_error)))
    self.assertRaises(ValueError, race.Get)

    # Test values with except_pass.
    def throws_except_error():
      raise NotImplementedError()
    race = Race((Future(callback=throws_error),
                 Future(callback=throws_except_error)),
                 except_pass=(NotImplementedError,))
    self.assertRaises(ValueError, race.Get)

    race = Race((Future(callback=throws_error),
                 Future(callback=throws_error)),
                 except_pass=(ValueError,))
    self.assertRaises(ValueError, race.Get)

  def testThen(self):
    def assertIs42(val):
      self.assertEquals(val, 42)

    then = Future(value=42).Then(assertIs42)
    # Shouldn't raise an error.
    then.Get()

    # Test raising an error.
    then = Future(value=41).Then(assertIs42)
    self.assertRaises(AssertionError, then.Get)

    # Test setting up an error handler.
    def handle(error):
      if isinstance(error, ValueError):
        return 'Caught'
      raise error

    def raiseValueError():
      raise ValueError

    def raiseException():
      raise Exception

    then = Future(callback=raiseValueError).Then(assertIs42, handle)
    self.assertEquals(then.Get(), 'Caught')
    then = Future(callback=raiseException).Then(assertIs42, handle)
    self.assertRaises(Exception, then.Get)

    # Test chains of thens.
    addOne = lambda val: val + 1
    then = Future(value=40).Then(addOne).Then(addOne).Then(assertIs42)
    # Shouldn't raise an error.
    then.Get()

    # Test error in chain.
    then = Future(value=40).Then(addOne).Then(assertIs42).Then(addOne)
    self.assertRaises(AssertionError, then.Get)

    # Test handle error in chain.
    def raiseValueErrorWithVal(val):
      raise ValueError

    then = Future(value=40).Then(addOne).Then(raiseValueErrorWithVal).Then(
        addOne, handle).Then(lambda val: val + ' me')
    self.assertEquals(then.Get(), 'Caught me')

    # Test multiple handlers.
    def myHandle(error):
      if isinstance(error, AssertionError):
        return 10
      raise error

    then = Future(value=40).Then(assertIs42).Then(addOne, handle).Then(addOne,
                                                                       myHandle)
    self.assertEquals(then.Get(), 10)


if __name__ == '__main__':
  unittest.main()
