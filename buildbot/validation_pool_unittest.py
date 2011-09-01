# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains unittests for validation_pool module."""

import mox
import StringIO
import sys
import unittest
import urllib

import constants
sys.path.append(constants.SOURCE_ROOT)

from chromite.buildbot import validation_pool


class TestValidationPool(mox.MoxTestBase):
  """Tests methods in validation_pool.ValidationPool."""

  def _TreeStatusFile(self, message, general_state):
    """Returns a file-like object with the status message writtin in it."""
    my_response = self.mox.CreateMockAnything()
    my_response.json = '{"message": "%s", "general_state": "%s"}' % (
        message, general_state)
    return my_response

  def _TreeStatusTestHelper(self, tree_status, general_state, expected_return,
                            retries_500=0):
    """Tests whether we return the correct value based on tree_status."""
    return_status = self._TreeStatusFile(tree_status, general_state)
    self.mox.StubOutWithMock(urllib, 'urlopen')
    status_url = 'https://chromiumos-status.appspot.com/current?format=json'
    for _ in range(retries_500):
      urllib.urlopen(status_url).AndReturn(return_status)
      return_status.getcode().AndReturn(500)

    urllib.urlopen(status_url).AndReturn(return_status)
    return_status.getcode().AndReturn(200)
    return_status.read().AndReturn(return_status.json)
    self.mox.ReplayAll()
    self.assertEqual(validation_pool.ValidationPool._IsTreeOpen(),
                     expected_return)
    self.mox.VerifyAll()

  def testTreeIsOpen(self):
    """Tests that we return True is the tree is open."""
    self._TreeStatusTestHelper('Tree is open (flaky bug on flaky builder)',
                               'open', True)

  def testTreeIsClosed(self):
    """Tests that we return false is the tree is closed."""
    self._TreeStatusTestHelper('Tree is closed (working on a patch)', 'closed',
                               False)

  def testTreeIsThrottled(self):
    """Tests that we return false is the tree is throttled."""
    self._TreeStatusTestHelper('Tree is throttled (waiting to cycle)',
                               'throttled', True)

  def testTreeStatusWithNetworkFailures(self):
    """Checks for non-500 errors.."""
    self._TreeStatusTestHelper('Tree is open (flaky bug on flaky builder)',
                               'open', True, retries_500=2)


if __name__ == '__main__':
  unittest.main()
