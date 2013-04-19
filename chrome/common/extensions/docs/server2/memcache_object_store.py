# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from appengine_wrappers import memcache
from object_store import ObjectStore

class _AsyncMemcacheGetFuture(object):
  def __init__(self, rpc):
    self._rpc = rpc

  def Get(self):
    return self._rpc.get_result()

class MemcacheObjectStore(ObjectStore):
  def __init__(self, namespace):
    self._namespace = namespace

  def SetMulti(self, mapping):
    try:
      memcache.Client().set_multi_async(mapping, namespace=self._namespace)
    except ValueError as e:
      logging.error('Caught "ValueError: %s" when mapping keys %s' % (
          e, mapping.keys()))

  def GetMulti(self, keys):
    rpc = memcache.Client().get_multi_async(keys, namespace=self._namespace)
    return _AsyncMemcacheGetFuture(rpc)

  def DelMulti(self, keys):
    memcache.delete_multi(keys, namespace=self._namespace)
