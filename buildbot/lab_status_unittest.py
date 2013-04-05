#!/usr/bin/python

# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for lab status."""

import constants
from mock import Mock
import sys
import time
import urllib

sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.buildbot import lab_status
from chromite.lib import cros_test_lib


class TestLabStatus(cros_test_lib.MockTestCase):
  """Class that tests GetLabStatus and CheckLabStatus."""

  def setUp(self):
    self.PatchObject(time, 'sleep')
    self.PatchObject(urllib, 'urlopen')

  def _LabStatusFile(self, message, general_state):
    """Returns a file-like object with the status message written in it."""
    my_response = Mock()
    my_response.json = '{"message": "%s", "general_state": "%s"}' % (
        message, general_state)
    return my_response

  def _TestGetLabStatusHelper(self, lab_message, general_state, expected_return,
                              max_attempts=5, failed_attempts=0):
    """Tests whether we get correct lab status.
    Args:
      lab_message: A message describing lab status and
                   disabled boards, e.g. "Lab is Up [stumpy, kiev]"
      general_state: Current lab state, e.g. 'open'.
      expected_return: The expected return of GetLabStatus,
                       e.g. {'lab_is_up': True, 'message': 'Lab is up'}.
      max_attempts: Max attempts GetLabStatus will make to get lab status.
      failed_attempts: Number of failed attempts we want to mock.
    """
    return_status = self._LabStatusFile(lab_message, general_state)
    urlopen_side_effect = [500 for _ in range(failed_attempts)]
    if failed_attempts < max_attempts:
      urlopen_side_effect.append(200)
      call_count = failed_attempts + 1
    else:
      call_count = max_attempts
    return_status.getcode.side_effect = urlopen_side_effect
    urllib.urlopen.return_value = return_status
    return_status.read.return_value = return_status.json
    self.assertEqual(lab_status.GetLabStatus(max_attempts), expected_return)
    # pylint: disable=E1101
    self.assertEqual(urllib.urlopen.call_count, call_count)

  def testGetLabStatusWithOneAttempt(self):
    """Tests that GetLabStatus succeeds with one attempt."""
    expected_return = {'lab_is_up': True, 'message': 'Lab is up'}
    self._TestGetLabStatusHelper('Lab is up', 'open', expected_return)

  def testGetLabStatusWithMultipleAttempts(self):
    """Tests that GetLabStatus succeeds after multiple tries."""
    expected_return = {'lab_is_up': True, 'message': 'Lab is up'}
    self._TestGetLabStatusHelper('Lab is up', 'open', expected_return,
                              max_attempts=5, failed_attempts=3)

  def testGetLabStatusFailsWithMultipleAttempts(self):
    """Tests that GetLabStatus fails after multiple tries."""
    expected_return = {'lab_is_up': True, 'message': ''}
    self._TestGetLabStatusHelper('dummy_msg', 'dummy_state', expected_return,
                              max_attempts=5, failed_attempts=5)

  def _BuildLabStatus(self, lab_is_up, disabled_boards=None):
    """Build a dictionary representing the lab status."""
    if lab_is_up:
      message = 'Lab is Open'
      if disabled_boards:
        message += '[%s]' % (', '.join(disabled_boards))
    else:
      message = 'Lab is Down (For some reason it is down.)'

    status = {'lab_is_up': lab_is_up, 'message': message}
    return status

  def _TestCheckLabStatusHelper(self, lab_is_up):
    """Tests CheckLabStatus runs properly."""
    self.PatchObject(lab_status, 'GetLabStatus')
    status = self._BuildLabStatus(lab_is_up)
    lab_status.GetLabStatus.return_value = status
    if lab_is_up:
      lab_status.CheckLabStatus()
    else:
      self.assertRaises(lab_status.LabIsDownException,
                        lab_status.CheckLabStatus)

  def testCheckLabStatusWhenLabUp(self):
    """Tests CheckLabStatus runs properly when lab is up."""
    self._TestCheckLabStatusHelper(True)

  def testCheckLabStatusWhenLabDown(self):
    """Tests CheckLabStatus runs properly when lab is down."""
    self._TestCheckLabStatusHelper(False)

  def testCheckLabStatusWhenBoardsDisabled(self):
    """Tests CheckLabStatus runs properly when some boards are disabled."""
    self.PatchObject(lab_status, 'GetLabStatus')
    status = self._BuildLabStatus(True, ['stumpy', 'kiev', 'x85-alex'])
    lab_status.GetLabStatus.return_value = status
    lab_status.CheckLabStatus('lumpy')
    self.assertRaises(lab_status.BoardIsDisabledException,
                      lab_status.CheckLabStatus,
                      'stumpy')


if __name__ == '__main__':
  cros_test_lib.main()
