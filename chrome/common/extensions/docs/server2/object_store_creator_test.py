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
    self.creator = ObjectStoreCreator(_FooClass, store_type=TestObjectStore)

  def testVanilla(self):
    store = self.creator.Create()
    self.assertEqual('_FooClass', store.namespace)

  def testWithVersion(self):
    store = self.creator.Create(version=42)
    self.assertEqual('_FooClass/42', store.namespace)

  def testWithCategory(self):
    store = self.creator.Create(category='cat')
    self.assertEqual('_FooClass/cat', store.namespace)

  def testWithVersionAndCategory(self):
    store = self.creator.Create(version=43, category='mat')
    self.assertEqual('_FooClass/mat/43', store.namespace)

  def testIllegalIinput(self):
    self.assertRaises(self.creator.Create, category='5')
    self.assertRaises(self.creator.Create, category='forty2')
    self.assertRaises(self.creator.Create, version='twenty')
    self.assertRaises(self.creator.Create, version='7a')

if __name__ == '__main__':
  unittest.main()
