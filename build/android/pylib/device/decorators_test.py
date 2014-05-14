# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Unit tests for decorators.py.
"""

# pylint: disable=W0613

import time
import traceback
import unittest

from pylib.device import decorators
from pylib.device import device_errors

_DEFAULT_TIMEOUT = 30
_DEFAULT_RETRIES = 3

class DecoratorsTest(unittest.TestCase):
  _decorated_function_called_count = 0

  def testFunctionDecoratorDoesTimeouts(self):
    """Tests that the base decorator handles the timeout logic."""
    DecoratorsTest._decorated_function_called_count = 0
    @decorators.WithTimeoutAndRetries
    def alwaysTimesOut(timeout=None, retries=None):
      DecoratorsTest._decorated_function_called_count += 1
      time.sleep(100)

    start_time = time.time()
    with self.assertRaises(device_errors.CommandTimeoutError):
      alwaysTimesOut(timeout=1, retries=0)
    elapsed_time = time.time() - start_time
    self.assertTrue(elapsed_time >= 1)
    self.assertEquals(1, DecoratorsTest._decorated_function_called_count)

  def testFunctionDecoratorDoesRetries(self):
    """Tests that the base decorator handles the retries logic."""
    DecoratorsTest._decorated_function_called_count = 0
    @decorators.WithTimeoutAndRetries
    def alwaysRaisesCommandFailedError(timeout=None, retries=None):
      DecoratorsTest._decorated_function_called_count += 1
      raise device_errors.CommandFailedError(['testCommand'],
                                             'testCommand failed')

    with self.assertRaises(device_errors.CommandFailedError):
      alwaysRaisesCommandFailedError(timeout=30, retries=10)
    self.assertEquals(11, DecoratorsTest._decorated_function_called_count)

  def testFunctionDecoratorRequiresParams(self):
    """Tests that the base decorator requires timeout and retries params."""
    @decorators.WithTimeoutAndRetries
    def requiresExplicitTimeoutAndRetries(timeout=None, retries=None):
      return (timeout, retries)

    with self.assertRaises(KeyError):
      requiresExplicitTimeoutAndRetries()
    with self.assertRaises(KeyError):
      requiresExplicitTimeoutAndRetries(timeout=10)
    with self.assertRaises(KeyError):
      requiresExplicitTimeoutAndRetries(retries=0)
    expected_timeout = 10
    expected_retries = 1
    (actual_timeout, actual_retries) = (
        requiresExplicitTimeoutAndRetries(timeout=expected_timeout,
                                          retries=expected_retries))
    self.assertEquals(expected_timeout, actual_timeout)
    self.assertEquals(expected_retries, actual_retries)

  def testDefaultsFunctionDecoratorDoesTimeouts(self):
    """Tests that the defaults decorator handles timeout logic."""
    DecoratorsTest._decorated_function_called_count = 0
    @decorators.WithTimeoutAndRetriesDefaults(1, 0)
    def alwaysTimesOut(timeout=None, retries=None):
      DecoratorsTest._decorated_function_called_count += 1
      time.sleep(100)

    start_time = time.time()
    with self.assertRaises(device_errors.CommandTimeoutError):
      alwaysTimesOut()
    elapsed_time = time.time() - start_time
    self.assertTrue(elapsed_time >= 1)
    self.assertEquals(1, DecoratorsTest._decorated_function_called_count)

    DecoratorsTest._decorated_function_called_count = 0
    with self.assertRaises(device_errors.CommandTimeoutError):
      alwaysTimesOut(timeout=2)
    elapsed_time = time.time() - start_time
    self.assertTrue(elapsed_time >= 2)
    self.assertEquals(1, DecoratorsTest._decorated_function_called_count)

  def testDefaultsFunctionDecoratorDoesRetries(self):
    """Tests that the defaults decorator handles retries logic."""
    DecoratorsTest._decorated_function_called_count = 0
    @decorators.WithTimeoutAndRetriesDefaults(30, 10)
    def alwaysRaisesCommandFailedError(timeout=None, retries=None):
      DecoratorsTest._decorated_function_called_count += 1
      raise device_errors.CommandFailedError(['testCommand'],
                                             'testCommand failed')

    with self.assertRaises(device_errors.CommandFailedError):
      alwaysRaisesCommandFailedError()
    self.assertEquals(11, DecoratorsTest._decorated_function_called_count)

    DecoratorsTest._decorated_function_called_count = 0
    with self.assertRaises(device_errors.CommandFailedError):
      alwaysRaisesCommandFailedError(retries=5)
    self.assertEquals(6, DecoratorsTest._decorated_function_called_count)

  def testExplicitFunctionDecoratorDoesTimeouts(self):
    """Tests that the explicit decorator handles timeout logic."""
    DecoratorsTest._decorated_function_called_count = 0
    @decorators.WithExplicitTimeoutAndRetries(1, 0)
    def alwaysTimesOut():
      DecoratorsTest._decorated_function_called_count += 1
      time.sleep(100)

    start_time = time.time()
    with self.assertRaises(device_errors.CommandTimeoutError):
      alwaysTimesOut()
    elapsed_time = time.time() - start_time
    self.assertTrue(elapsed_time >= 1)
    self.assertEquals(1, DecoratorsTest._decorated_function_called_count)

  def testExplicitFunctionDecoratorDoesRetries(self):
    """Tests that the explicit decorator handles retries logic."""
    DecoratorsTest._decorated_function_called_count = 0
    @decorators.WithExplicitTimeoutAndRetries(30, 10)
    def alwaysRaisesCommandFailedError():
      DecoratorsTest._decorated_function_called_count += 1
      raise device_errors.CommandFailedError(['testCommand'],
                                             'testCommand failed')

    with self.assertRaises(device_errors.CommandFailedError):
      alwaysRaisesCommandFailedError()
    self.assertEquals(11, DecoratorsTest._decorated_function_called_count)

  class _MethodDecoratorTestObject(object):
    """An object suitable for testing the method decorator."""

    def __init__(self, test_case, default_timeout=_DEFAULT_TIMEOUT,
                 default_retries=_DEFAULT_RETRIES):
      self._test_case = test_case
      self.default_timeout = default_timeout
      self.default_retries = default_retries
      self.function_call_counters = {
          'alwaysRaisesCommandFailedError': 0,
          'alwaysTimesOut': 0,
          'requiresExplicitTimeoutAndRetries': 0,
      }

    @decorators.WithTimeoutAndRetriesFromInstance(
        'default_timeout', 'default_retries')
    def alwaysTimesOut(self, timeout=None, retries=None):
      self.function_call_counters['alwaysTimesOut'] += 1
      time.sleep(100)
      self._test_case.assertFalse(True, msg='Failed to time out?')

    @decorators.WithTimeoutAndRetriesFromInstance(
        'default_timeout', 'default_retries')
    def alwaysRaisesCommandFailedError(self, timeout=None, retries=None):
      self.function_call_counters['alwaysRaisesCommandFailedError'] += 1
      raise device_errors.CommandFailedError(['testCommand'],
                                             'testCommand failed')


  def testMethodDecoratorDoesTimeout(self):
    """Tests that the method decorator handles timeout logic."""
    test_obj = self._MethodDecoratorTestObject(self)
    start_time = time.time()
    with self.assertRaises(device_errors.CommandTimeoutError):
      try:
        test_obj.alwaysTimesOut(timeout=1, retries=0)
      except:
        traceback.print_exc()
        raise
    elapsed_time = time.time() - start_time
    self.assertTrue(elapsed_time >= 1)
    self.assertEquals(1, test_obj.function_call_counters['alwaysTimesOut'])

  def testMethodDecoratorDoesRetries(self):
    """ Tests that the method decorator handles retries logic."""
    test_obj = self._MethodDecoratorTestObject(self)
    with self.assertRaises(device_errors.CommandFailedError):
      try:
        test_obj.alwaysRaisesCommandFailedError(retries=10)
      except:
        traceback.print_exc()
        raise
    self.assertEquals(
        11, test_obj.function_call_counters['alwaysRaisesCommandFailedError'])


if __name__ == '__main__':
  unittest.main()


