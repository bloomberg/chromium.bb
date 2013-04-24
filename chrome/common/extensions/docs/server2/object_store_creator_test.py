#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from test_object_store import TestObjectStore
from object_store_creator import ObjectStoreCreator

class _FooClass(object):
  def __init__(self): pass

class ObjectStoreCreatorTest(unittest.TestCase):
  def setUp(self):
    self.creator = ObjectStoreCreator(_FooClass,
                                      '3-0',
                                      'test',
                                      store_type=TestObjectStore)

  def testVanilla(self):
    store = self.creator.Create()
    self.assertEqual('3-0/_FooClass@test', store.namespace)

  def testWithCategory(self):
    store = self.creator.Create(category='cat')
    self.assertEqual('3-0/_FooClass@test/cat', store.namespace)

  def testIllegalInput(self):
    self.assertRaises(AssertionError, self.creator.Create, category='5')
    self.assertRaises(AssertionError, self.creator.Create, category='forty2')

  def testFactoryWithBranch(self):
    store = ObjectStoreCreator.Factory('3-0', 'dev').Create(
        _FooClass, store_type=TestObjectStore).Create()
    self.assertEqual('3-0/_FooClass@dev', store.namespace)

if __name__ == '__main__':
  unittest.main()
