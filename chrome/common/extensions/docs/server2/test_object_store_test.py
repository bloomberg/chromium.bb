#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from test_object_store import TestObjectStore
import unittest

class TestObjectStoreTest(unittest.TestCase):
  def testEmpty(self):
    store = TestObjectStore('namespace')
    self.assertEqual(None, store.Get('hi').Get())
    self.assertEqual({}, store.GetMulti(['hi', 'lo']).Get())

  def testNonEmpty(self):
    store = TestObjectStore('namespace')
    store.Set('hi', 'bye')
    self.assertEqual('bye', store.Get('hi').Get())
    self.assertEqual({'hi': 'bye'}, store.GetMulti(['hi', 'lo']).Get())
    store.Set('hi', 'blah')
    self.assertEqual('blah', store.Get('hi').Get())
    self.assertEqual({'hi': 'blah'}, store.GetMulti(['hi', 'lo']).Get())
    store.Delete('hi')
    self.assertEqual(None, store.Get('hi').Get())
    self.assertEqual({}, store.GetMulti(['hi', 'lo']).Get())

if __name__ == '__main__':
  unittest.main()
