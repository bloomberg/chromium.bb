# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from appengine_wrappers import memcache
from object_store import ObjectStore, CACHE_TIMEOUT

class _AsyncMemcacheGetFuture(object):
  def __init__(self, rpc):
    self._rpc = rpc

  def Get(self):
    return self._rpc.get_result()

class MemcacheObjectStore(ObjectStore):
  def SetMulti(self, mapping, namespace, time=CACHE_TIMEOUT):
    memcache.Client().set_multi_async(mapping, namespace=namespace, time=time)

  def GetMulti(self, keys, namespace, time=CACHE_TIMEOUT):
    rpc = memcache.Client().get_multi_async(keys, namespace=namespace)
    return _AsyncMemcacheGetFuture(rpc)

  def Delete(self, key, namespace):
    memcache.delete(key, namespace=namespace)
