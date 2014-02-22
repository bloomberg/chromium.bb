#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test suite for timeout_util.py"""

import datetime
import os
import sys
import time
import urllib

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.buildbot import constants
from chromite.lib import cros_test_lib
from chromite.lib import timeout_util


# pylint: disable=W0212,R0904


class TestTimeouts(cros_test_lib.TestCase):
  """Tests for timeout_util.Timeout."""

  def testTimeout(self):
    """Tests that we can nest Timeout correctly."""
    self.assertFalse('mock' in str(time.sleep).lower())
    with timeout_util.Timeout(30):
      with timeout_util.Timeout(20):
        with timeout_util.Timeout(1):
          self.assertRaises(timeout_util.TimeoutError, time.sleep, 10)

        # Should not raise a timeout exception as 20 > 2.
        time.sleep(1)

  def testTimeoutNested(self):
    """Tests that we still re-raise an alarm if both are reached."""
    with timeout_util.Timeout(1):
      try:
        with timeout_util.Timeout(2):
          self.assertRaises(timeout_util.TimeoutError, time.sleep, 1)

      # Craziness to catch nested timeouts.
      except timeout_util.TimeoutError:
        pass
      else:
        self.assertTrue(False, 'Should have thrown an exception')


class TestWaitFors(cros_test_lib.TestCase):
  """Tests for assorted timeout_utils WaitForX methods."""

  def setUp(self):
    self.values_ix = 0
    self.timestart = None
    self.timestop = None

  def GetFunc(self, return_values):
    """Return a functor that returns given values in sequence with each call."""
    self.values_ix = 0
    self.timestart = None
    self.timestop = None

    def _Func():
      if not self.timestart:
        self.timestart = datetime.datetime.utcnow()

      val = return_values[self.values_ix]
      self.values_ix += 1

      self.timestop = datetime.datetime.utcnow()
      return val

    return _Func

  def GetTryCount(self):
    """Get number of times func was tried."""
    return self.values_ix

  def GetTrySeconds(self):
    """Get number of seconds that span all func tries."""
    delta = self.timestop - self.timestart
    return int(delta.seconds + 0.5)

  def _TestWaitForSuccess(self, maxval, timeout, **kwargs):
    """Run through a test for WaitForSuccess."""

    func = self.GetFunc(range(20))
    def _RetryCheck(val):
      return val < maxval

    return timeout_util.WaitForSuccess(_RetryCheck, func, timeout, **kwargs)

  def _TestWaitForReturnValue(self, values, timeout, **kwargs):
    """Run through a test for WaitForReturnValue."""
    func = self.GetFunc(range(20))
    return timeout_util.WaitForReturnValue(values, func, timeout, **kwargs)

  def testWaitForSuccess1(self):
    """Test success after a few tries."""
    self.assertEquals(4, self._TestWaitForSuccess(4, 10, period=1))
    self.assertEquals(5, self.GetTryCount())
    self.assertEquals(4, self.GetTrySeconds())

  def testWaitForSuccess2(self):
    """Test timeout after a couple tries."""
    self.assertRaises(timeout_util.TimeoutError, self._TestWaitForSuccess,
                      4, 3, period=1)
    self.assertEquals(3, self.GetTryCount())
    self.assertEquals(2, self.GetTrySeconds())

  def testWaitForSuccess3(self):
    """Test success on first try."""
    self.assertEquals(0, self._TestWaitForSuccess(0, 10, period=1))
    self.assertEquals(1, self.GetTryCount())
    self.assertEquals(0, self.GetTrySeconds())

  def testWaitForSuccess4(self):
    """Test success after a few tries with longer period."""
    self.assertEquals(3, self._TestWaitForSuccess(3, 10, period=2))
    self.assertEquals(4, self.GetTryCount())
    self.assertEquals(6, self.GetTrySeconds())

  def testWaitForReturnValue1(self):
    """Test value found after a few tries."""
    self.assertEquals(4, self._TestWaitForReturnValue((4, 5), 10, period=1))
    self.assertEquals(5, self.GetTryCount())
    self.assertEquals(4, self.GetTrySeconds())

  def testWaitForReturnValue2(self):
    """Test value found on first try."""
    self.assertEquals(0, self._TestWaitForReturnValue((0, 1), 10, period=1))
    self.assertEquals(1, self.GetTryCount())
    self.assertEquals(0, self.GetTrySeconds())


class TestTreeStatus(cros_test_lib.MoxTestCase):
  """Tests TreeStatus method in cros_build_lib."""

  status_url = 'https://chromiumos-status.appspot.com/current?format=json'

  def setUp(self):
    pass

  def _TreeStatusFile(self, message, general_state):
    """Returns a file-like object with the status message writtin in it."""
    my_response = self.mox.CreateMockAnything()
    my_response.json = '{"message": "%s", "general_state": "%s"}' % (
        message, general_state)
    return my_response

  def _SetupMockTreeStatusResponses(self, status_url,
                                    final_tree_status='Tree is open.',
                                    final_general_state=constants.TREE_OPEN,
                                    rejected_tree_status='Tree is closed.',
                                    rejected_general_state=
                                    constants.TREE_CLOSED,
                                    rejected_status_count=0,
                                    retries_500=0,
                                    output_final_status=True):
    """Mocks out urllib.urlopen commands to simulate a given tree status.

    Args:
      status_url: The status url that status will be fetched from.
      final_tree_status: The final value of tree status that will be returned
        by urlopen.
      final_general_state: The final value of 'general_state' that will be
        returned by urlopen.
      rejected_tree_status: An intermediate value of tree status that will be
        returned by urlopen and retried upon.
      rejected_general_state: An intermediate value of 'general_state' that
        will be returned by urlopen and retried upon.
      rejected_status_count: The number of times urlopen will return the
        rejected state.
      retries_500: The number of times urlopen will fail with a 500 code.
      output_final_status: If True, the status given by final_tree_status and
        final_general_state will be the last status returned by urlopen. If
        False, final_tree_status will never be returned, and instead an
        unlimited number of times rejected_response will be returned.
    """

    final_response = self._TreeStatusFile(final_tree_status,
                                          final_general_state)
    rejected_response = self._TreeStatusFile(rejected_tree_status,
                                            rejected_general_state)
    error_500_response = self.mox.CreateMockAnything()
    self.mox.StubOutWithMock(urllib, 'urlopen')

    for _ in range(retries_500):
      urllib.urlopen(status_url).AndReturn(error_500_response)
      error_500_response.getcode().AndReturn(500)

    if output_final_status:
      for _ in range(rejected_status_count):
        urllib.urlopen(status_url).AndReturn(rejected_response)
        rejected_response.getcode().AndReturn(200)
        rejected_response.read().AndReturn(rejected_response.json)

      urllib.urlopen(status_url).AndReturn(final_response)
      final_response.getcode().AndReturn(200)
      final_response.read().AndReturn(final_response.json)
    else:
      urllib.urlopen(status_url).MultipleTimes().AndReturn(rejected_response)
      rejected_response.getcode().MultipleTimes().AndReturn(200)
      rejected_response.read().MultipleTimes().AndReturn(
          rejected_response.json)

    self.mox.ReplayAll()

  def testTreeIsOpen(self):
    """Tests that we return True is the tree is open."""
    self._SetupMockTreeStatusResponses(self.status_url,
                                       rejected_status_count=5,
                                       retries_500=5)
    self.assertTrue(timeout_util.IsTreeOpen(self.status_url,
                                                   period=0))

  def testTreeIsClosed(self):
    """Tests that we return false is the tree is closed."""
    self._SetupMockTreeStatusResponses(self.status_url,
                                       output_final_status=False)
    self.assertFalse(timeout_util.IsTreeOpen(self.status_url,
                                                    period=0.1))

  def testTreeIsThrottled(self):
    """Tests that we return True if the tree is throttled."""
    self._SetupMockTreeStatusResponses(self.status_url,
        'Tree is throttled (flaky bug on flaky builder)',
        constants.TREE_THROTTLED)
    self.assertTrue(timeout_util.IsTreeOpen(self.status_url,
                    throttled_ok=True))

  def testTreeIsThrottledNotOk(self):
    """Tests that we respect throttled_ok"""
    self._SetupMockTreeStatusResponses(self.status_url,
      rejected_tree_status='Tree is throttled (flaky bug on flaky builder)',
      rejected_general_state=constants.TREE_THROTTLED,
      output_final_status=False)
    self.assertFalse(timeout_util.IsTreeOpen(self.status_url,
                                             period=0.1))

  def testWaitForStatusOpen(self):
    """Tests that we can wait for a tree open response."""
    self._SetupMockTreeStatusResponses(self.status_url)
    self.assertEqual(timeout_util.WaitForTreeStatus(self.status_url),
                     constants.TREE_OPEN)


  def testWaitForStatusThrottled(self):
    """Tests that we can wait for a tree open response."""
    self._SetupMockTreeStatusResponses(self.status_url,
        final_general_state=constants.TREE_THROTTLED)
    self.assertEqual(timeout_util.WaitForTreeStatus(self.status_url,
                                                    throttled_ok=True),
                     constants.TREE_THROTTLED)

  def testWaitForStatusFailure(self):
    """Tests that we can wait for a tree open response."""
    self._SetupMockTreeStatusResponses(self.status_url,
                                       output_final_status=False)
    self.assertRaises(timeout_util.TimeoutError,
                      timeout_util.WaitForTreeStatus, self.status_url,
                      period=0.1)


if __name__ == '__main__':
  cros_test_lib.main()
