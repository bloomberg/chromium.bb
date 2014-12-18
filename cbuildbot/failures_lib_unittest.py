# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the failures_lib module."""

from __future__ import print_function

from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.lib import cros_test_lib
from chromite.lib import fake_cidb


class CompoundFailureTest(cros_test_lib.TestCase):
  """Test the CompoundFailure class."""

  def _CreateExceptInfos(self, cls, message='', traceback='', num=1):
    """A helper function to create a list of ExceptInfo objects."""
    exc_infos = []
    for _ in xrange(num):
      exc_infos.extend(failures_lib.CreateExceptInfo(cls(message), traceback))

    return exc_infos

  def testHasEmptyList(self):
    """Tests the HasEmptyList method."""
    self.assertTrue(failures_lib.CompoundFailure().HasEmptyList())
    exc_infos = self._CreateExceptInfos(KeyError)
    self.assertFalse(
        failures_lib.CompoundFailure(exc_infos=exc_infos).HasEmptyList())

  def testHasAndMatchesFailureType(self):
    """Tests the HasFailureType and the MatchesFailureType methods."""
    # Create a CompoundFailure instance with mixed types of exceptions.
    exc_infos = self._CreateExceptInfos(KeyError)
    exc_infos.extend(self._CreateExceptInfos(ValueError))
    exc = failures_lib.CompoundFailure(exc_infos=exc_infos)
    self.assertTrue(exc.HasFailureType(KeyError))
    self.assertTrue(exc.HasFailureType(ValueError))
    self.assertFalse(exc.MatchesFailureType(KeyError))
    self.assertFalse(exc.MatchesFailureType(ValueError))

    # Create a CompoundFailure instance with a single type of exceptions.
    exc_infos = self._CreateExceptInfos(KeyError, num=5)
    exc = failures_lib.CompoundFailure(exc_infos=exc_infos)
    self.assertTrue(exc.HasFailureType(KeyError))
    self.assertFalse(exc.HasFailureType(ValueError))
    self.assertTrue(exc.MatchesFailureType(KeyError))
    self.assertFalse(exc.MatchesFailureType(ValueError))

  def testHasFatalFailure(self):
    """Tests the HasFatalFailure method."""
    exc_infos = self._CreateExceptInfos(KeyError)
    exc_infos.extend(self._CreateExceptInfos(ValueError))
    exc = failures_lib.CompoundFailure(exc_infos=exc_infos)
    self.assertTrue(exc.HasFatalFailure())
    self.assertTrue(exc.HasFatalFailure(whitelist=[KeyError]))
    self.assertFalse(exc.HasFatalFailure(whitelist=[KeyError, ValueError]))

    exc = failures_lib.CompoundFailure()
    self.assertFalse(exc.HasFatalFailure())

  def testMessageContainsAllInfo(self):
    """Tests that by default, all information is included in the message."""
    exc_infos = self._CreateExceptInfos(KeyError, message='bar1',
                                        traceback='foo1')
    exc_infos.extend(self._CreateExceptInfos(ValueError, message='bar2',
                                             traceback='foo2'))
    exc = failures_lib.CompoundFailure(exc_infos=exc_infos)
    self.assertTrue('bar1' in str(exc))
    self.assertTrue('bar2' in str(exc))
    self.assertTrue('KeyError' in str(exc))
    self.assertTrue('ValueError' in str(exc))
    self.assertTrue('foo1' in str(exc))
    self.assertTrue('foo2' in str(exc))

  def testReportStageFailureToCIDB(self):
    """Tests that the reporting fuction reports all included exceptions."""
    fake_db = fake_cidb.FakeCIDBConnection()
    inner_exception_1 = failures_lib.TestLabFailure()
    inner_exception_2 = TypeError()
    exc_infos = failures_lib.CreateExceptInfo(inner_exception_1, None)
    exc_infos += failures_lib.CreateExceptInfo(inner_exception_2, None)
    outer_exception = failures_lib.GoBFailure(exc_infos=exc_infos)

    mock_build_stage_id = 9345

    failures_lib.ReportStageFailureToCIDB(fake_db,
                                          mock_build_stage_id,
                                          outer_exception)
    self.assertEqual(3, len(fake_db.failureTable))
    self.assertEqual(
        set([mock_build_stage_id]),
        set([x['build_stage_id'] for x in fake_db.failureTable.values()]))
    self.assertEqual(
        set([constants.EXCEPTION_CATEGORY_INFRA,
             constants.EXCEPTION_CATEGORY_UNKNOWN,
             constants.EXCEPTION_CATEGORY_LAB]),
        set([x['exception_category'] for x in fake_db.failureTable.values()]))

    # Find the outer failure id.
    for failure_id, failure in fake_db.failureTable.iteritems():
      if failure['outer_failure_id'] is None:
        outer_failure_id = failure_id
        break

    # Now verify inner failures reference this failure.
    for failure_id, failure in fake_db.failureTable.iteritems():
      if failure_id != outer_failure_id:
        self.assertEqual(outer_failure_id, failure['outer_failure_id'])


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
    org_infos = failures_lib.CreateExceptInfo(ValueError('No taco.'), tb1) + \
                failures_lib.CreateExceptInfo(OSError('No salsa'), tb2)
    try:
      self._GetFunction(self.SubparLunch, self.TacoNotTasty,
                        exc_infos=org_infos)()
    except Exception as e:
      self.assertTrue(isinstance(e, self.SubparLunch))
      # The orignal exceptions stored in exc_infos are preserved.
      self.assertEqual(e.exc_infos, org_infos)
      # All essential inforamtion should be included in the message of
      # the new excpetion.
      self.assertTrue(tb1 in str(e))
      self.assertTrue(tb2 in str(e))
      self.assertTrue(str(ValueError) in str(e))
      self.assertTrue(str(OSError) in str(e))
      self.assertTrue(str('No taco') in str(e))
      self.assertTrue(str('No salsa') in str(e))

      # Assert that summary does not contain the textual tracebacks.
      self.assertFalse(tb1 in e.ToSummaryString())
      self.assertFalse(tb2 in e.ToSummaryString())

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

  def testPassArgsToWrappedFunctor(self):
    """Tests that we can pass arguments to the functor."""
    @failures_lib.SetFailureType(self.TacoNotTasty)
    def f(arg):
      return arg

    @failures_lib.SetFailureType(self.TacoNotTasty)
    def g(kwarg=''):
      return kwarg

    # Test passing arguments.
    self.assertEqual(f('foo'), 'foo')
    # Test passing keyword arguments.
    self.assertEqual(g(kwarg='bar'), 'bar')


class ExceptInfoTest(cros_test_lib.TestCase):
  """Tests the namedtuple class ExceptInfo."""

  def testConvertToExceptInfo(self):
    """Tests converting an exception to an ExceptInfo object."""
    traceback = 'Dummy traceback'
    message = 'Taco is not a valid option!'
    except_infos = failures_lib.CreateExceptInfo(
        ValueError(message), traceback)

    self.assertEqual(except_infos[0].type, ValueError)
    self.assertEqual(except_infos[0].str, message)
    self.assertEqual(except_infos[0].traceback, traceback)
