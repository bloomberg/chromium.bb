#!/usr/bin/python

# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the cbuildbot_failures module."""

import os
import sys

sys.path.insert(0, os.path.abspath('%s/../..' % os.path.dirname(__file__)))
from chromite.cbuildbot import cbuildbot_failures as failures_lib
from chromite.lib import cros_test_lib


class SetFailureTypeTest(cros_test_lib.TestCase):
  """Test that the SetFailureType decorator works."""
  ERROR_MESSAGE = 'You failed!'

  class TacoNotTasty(failures_lib.CompoundFailure):
    """Raised when the taco is not tasty."""

  class NoGuacamole(TacoNotTasty):
    """Raised when no guacamole in the taco."""

  class SubparLunch(failures_lib.CompoundFailure):
    """Raised when the lunch is subpar."""

  class FooException(Exception):
    """A foo exception."""

  def _GetFunction(self, set_type, raise_type, *args, **kwargs):
    """Returns a function to test.

    Args:
      set_type: The exception type that the function is decorated with.
      raise_type: The exception type that the function raises.
      *args: args to pass to the instance of |raise_type|.

    Returns:
      The function to test.
    """
    @failures_lib.SetFailureType(set_type)
    def f():
      raise raise_type(*args, **kwargs)

    return f

  def testAssertionFailOnIllegalExceptionType(self):
    """Assertion should fail if the pre-set type is not allowed ."""
    self.assertRaises(AssertionError, self._GetFunction, ValueError,
                      self.FooException)

  def testReraiseAsNewException(self):
    """Tests that the pre-set exception type is raised correctly."""
    try:
      self._GetFunction(self.TacoNotTasty, self.FooException,
                        self.ERROR_MESSAGE)()
    except Exception as e:
      self.assertTrue(isinstance(e, self.TacoNotTasty))
      self.assertTrue(e.message, self.ERROR_MESSAGE)
      self.assertEqual(len(e.exc_infos), 1)
      self.assertEqual(e.exc_infos[0].str, self.ERROR_MESSAGE)
      self.assertEqual(e.exc_infos[0].type, self.FooException)
      self.assertTrue(isinstance(e.exc_infos[0].traceback, str))

  def testReraiseACompoundFailure(self):
    """Tests that the list of ExceptInfo objects are copied over."""
    tb1 = 'Dummy traceback1'
    tb2 = 'Dummy traceback2'
    org_exc_infos = [failures_lib.CreateExceptInfo(ValueError('No taco.'), tb1),
                     failures_lib.CreateExceptInfo(OSError('No salsa'), tb2)]
    try:
      self._GetFunction(self.SubparLunch, self.TacoNotTasty,
                        exc_infos=org_exc_infos)()
    except Exception as e:
      self.assertTrue(isinstance(e, self.SubparLunch))
      # The orignal exceptions stored in exc_infos are preserved.
      self.assertEqual(e.exc_infos, org_exc_infos)
      # All essential inforamtion should be included in the message of
      # the new excpetion.
      self.assertTrue(tb1 in str(e))
      self.assertTrue(tb2 in str(e))
      self.assertTrue(str(ValueError) in str(e))
      self.assertTrue(str(OSError) in str(e))
      self.assertTrue(str('No taco') in str(e))
      self.assertTrue(str('No salsa') in str(e))

  def testReraiseACompoundFailureWithEmptyList(self):
    """Tests that a CompoundFailure with empty list is handled correctly."""
    try:
      self._GetFunction(self.SubparLunch, self.TacoNotTasty,
                        message='empty list')()
    except Exception as e:
      self.assertTrue(isinstance(e, self.SubparLunch))
      self.assertEqual(e.exc_infos[0].type, self.TacoNotTasty)

  def testReraiseOriginalException(self):
    """Tests that the original exception is re-raised."""
    # NoGuacamole is a subclass of TacoNotTasty, so the wrapper has no
    # effect on it.
    f = self._GetFunction(self.TacoNotTasty, self.NoGuacamole)
    self.assertRaises(self.NoGuacamole, f)


class ExceptInfoTest(cros_test_lib.TestCase):
  """Tests the namedtuple class ExceptInfo."""

  def testConvertToExceptInfo(self):
    """Tests converting an exception to an ExceptInfo object."""
    traceback = 'Dummy traceback'
    message = 'Taco is not a valid option!'
    except_info = failures_lib.CreateExceptInfo(
        ValueError(message), traceback)

    self.assertEqual(except_info.type, ValueError)
    self.assertEqual(except_info.str, message)
    self.assertEqual(except_info.traceback, traceback)


if __name__ == '__main__':
  cros_test_lib.main()
