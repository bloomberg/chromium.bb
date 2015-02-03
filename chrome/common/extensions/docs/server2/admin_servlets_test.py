#!/usr/bin/env python
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from admin_servlets import ResetCommitServlet
from commit_tracker import CommitTracker
from object_store_creator import ObjectStoreCreator
from servlet import Request


_COMMIT_HISTORY_DATA = (
  '1234556789abcdef1234556789abcdef12345567',
  'f00f00f00f00f00f00f00f00f00f00f00f00f00f',
  '1010101010101010101010101010101010101010',
  'abcdefabcdefabcdefabcdefabcdefabcdefabcd',
  '4242424242424242424242424242424242424242',
)


class _ResetCommitDelegate(ResetCommitServlet.Delegate):
  def __init__(self, commit_tracker):
    self._commit_tracker = commit_tracker

  def CreateCommitTracker(self):
    return self._commit_tracker


class AdminServletsTest(unittest.TestCase):
  def setUp(self):
    object_store_creator = ObjectStoreCreator(start_empty=True)
    self._commit_tracker = CommitTracker(object_store_creator)
    for id in _COMMIT_HISTORY_DATA:
      self._commit_tracker.Set('master', id).Get()

  def _ResetCommit(self, commit_name, commit_id):
    return ResetCommitServlet(
        Request.ForTest('%s/%s' % (commit_name, commit_id)),
        _ResetCommitDelegate(self._commit_tracker)).Get()

  def _AssertBadRequest(self, commit_name, commit_id):
    response = self._ResetCommit(commit_name, commit_id)
    self.assertEqual(response.status, 400,
        'Should have failed to reset to commit %s to %s.' %
        (commit_name, commit_id))

  def _AssertOk(self, commit_name, commit_id):
    response = self._ResetCommit(commit_name, commit_id)
    self.assertEqual(response.status, 200,
        'Failed to reset commit %s to %s.' % (commit_name, commit_id))

  def testResetCommitServlet(self):
    # Make sure all the valid commits can be used for reset.
    for id in _COMMIT_HISTORY_DATA:
      self._AssertOk('master', id)

    # Non-existent commit should fail to update
    self._AssertBadRequest('master',
                           'b000000000000000000000000000000000000000')

    # Commit 'master' should still point to the last valid entry
    self.assertEqual(self._commit_tracker.Get('master').Get(),
                     _COMMIT_HISTORY_DATA[-1])

    # Reset to a valid commit but older
    self._AssertOk('master', _COMMIT_HISTORY_DATA[0])

    # Commit 'master' should point to the first history entry
    self.assertEqual(self._commit_tracker.Get('master').Get(),
                     _COMMIT_HISTORY_DATA[0])

    # Add a new entry to the history and validate that it can be used for reset.
    _NEW_ENTRY = '9999999999999999999999999999999999999999'
    self._commit_tracker.Set('master', _NEW_ENTRY).Get()
    self._AssertOk('master', _NEW_ENTRY)

    # Add a bunch (> 50) of entries to ensure that _NEW_ENTRY has been flushed
    # out of the history.
    for i in xrange(0, 20):
      for id in _COMMIT_HISTORY_DATA:
        self._commit_tracker.Set('master', id).Get()

    # Verify that _NEW_ENTRY is no longer valid for reset.
    self._AssertBadRequest('master', _NEW_ENTRY)

if __name__ == '__main__':
  unittest.main()
