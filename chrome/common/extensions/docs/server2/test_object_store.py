# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from future import Future
from object_store import ObjectStore

class TestObjectStore(ObjectStore):
  '''An object store which records its namespace and behaves like a dict, for
  testing.
  '''
  def __init__(self, namespace):
    self.namespace = namespace
    self._store = {}

  def SetMulti(self, mapping, **optarg):
    self._store.update(mapping)

  def GetMulti(self, keys, **optargs):
    return Future(value=dict((k, v) for k, v in self._store.items()
                                    if k in keys))

  def Delete(self, key):
    del self._store[key]
