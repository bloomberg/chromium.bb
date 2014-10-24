#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from object_store_creator import ObjectStoreCreator
from refresh_tracker import RefreshTracker


class RefreshTrackerTest(unittest.TestCase):
  def setUp(self):
    self._refresh_tracker = RefreshTracker(ObjectStoreCreator.ForTest())

  def testNonExistentRefreshIsIncomplete(self):
    self.assertFalse(self._refresh_tracker.GetRefreshComplete('unicorns').Get())

  def testEmptyRefreshIsComplete(self):
    refresh_id = 'abcdefghijklmnopqrstuvwxyz'
    self._refresh_tracker.StartRefresh(refresh_id, []).Get()
    self.assertTrue(self._refresh_tracker.GetRefreshComplete(refresh_id).Get())

  def testRefreshCompletion(self):
    refresh_id = 'this is fun'
    self._refresh_tracker.StartRefresh(refresh_id, ['/do/foo', '/do/bar']).Get()
    self._refresh_tracker.MarkTaskComplete(refresh_id, '/do/foo').Get()
    self.assertFalse(self._refresh_tracker.GetRefreshComplete(refresh_id).Get())
    self._refresh_tracker.MarkTaskComplete(refresh_id, '/do/bar').Get()
    self.assertTrue(self._refresh_tracker.GetRefreshComplete(refresh_id).Get())

  def testUnknownTasksAreIrrelevant(self):
    refresh_id = 'i am a banana'
    self._refresh_tracker.StartRefresh(refresh_id, ['a', 'b', 'c', 'd']).Get()
    self._refresh_tracker.MarkTaskComplete(refresh_id, 'a').Get()
    self._refresh_tracker.MarkTaskComplete(refresh_id, 'b').Get()
    self._refresh_tracker.MarkTaskComplete(refresh_id, 'c').Get()
    self._refresh_tracker.MarkTaskComplete(refresh_id, 'q').Get()
    self.assertFalse(self._refresh_tracker.GetRefreshComplete(refresh_id).Get())
    self._refresh_tracker.MarkTaskComplete(refresh_id, 'd').Get()
    self.assertTrue(self._refresh_tracker.GetRefreshComplete(refresh_id).Get())


if __name__ == '__main__':
  unittest.main()
