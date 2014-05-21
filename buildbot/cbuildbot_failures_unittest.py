#!/usr/bin/python

# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the cbuildbot_failures module."""

import os
import sys

sys.path.insert(0, os.path.abspath('%s/../..' % os.path.dirname(__file__)))
from chromite.buildbot import cbuildbot_failures as failures_lib
from chromite.lib import cros_test_lib


class SetFailureTypeTest(cros_test_lib.TestCase):
  """Test that the SetFailureType decorator works."""
  ERROR_MESSAGE = 'You failed!'

  class TacoNotTasty(Exception):
    """Raised when the taco is not tasty."""

  class NoGuacamole(TacoNotTasty):
    """Raised when no guacamole in the taco."""

  class FooException(Exception):
    """A foo exception."""

  def _GetFunction(self, set_type, raise_type):
    """Returns a function to test.

    Args:
      set_type: The exception type that the function is decorated with.
      raise_type: The exception type that the function raises.

    Returns:
      The function to test.
    """
    @failures_lib.SetFailureType(set_type)
    def f():
      raise raise_type(self.ERROR_MESSAGE)

    return f

  def testReraiseAsNewException(self):
    """Tests that the pre-set exception type is raised."""
    f = self._GetFunction(self.TacoNotTasty, self.FooException)
    self.assertRaises(self.TacoNotTasty, f)

  def testStepFailureRemainsPrintable(self):
    """Tests that the re-raised exception remains printable."""
    try:
      self._GetFunction(failures_lib.StepFailure, self.FooException)()
    except Exception as e:
      self.assertTrue(isinstance(e, failures_lib.StepFailure))
      self.assertTrue(isinstance(e.__str__(), str))

  def testReraiseOriginalException(self):
    """Tests that the original exception is re-raised."""
    f = self._GetFunction(self.TacoNotTasty, self.NoGuacamole)
    self.assertRaises(self.NoGuacamole, f)


if __name__ == '__main__':
  cros_test_lib.main()
