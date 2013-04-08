# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

from appengine_wrappers import CACHE_TIMEOUT
from future import Future
from object_store import ObjectStore

class _CacheEntry(object):
  def __init__(self, value, expire_time):
    self.value = value
    self._never_expires = (expire_time == 0)
    self._expiry = time.time() + expire_time

  def HasExpired(self):
    if self._never_expires:
      return False
    return time.time() > self._expiry

class _AsyncGetFuture(object):
  """A future for memcache gets.

  Properties:
  - |cache| the in-memory cache used by InMemoryObjectStore
  - |time| the cache timeout
  - |future| the |Future| from the backing |ObjectStore|
  - |initial_mapping| a mapping of cache items already in memory
  """
  def __init__(self, cache, time, future, initial_mapping):
    self._cache = cache
    self._time = time
    self._future = future
    self._mapping = initial_mapping

  def Get(self):
    if self._future is not None:
      result = self._future.Get()
      self._cache.update(
          dict((k, _CacheEntry(v, self._time)) for k, v in result.iteritems()))
      self._mapping.update(result)
    return self._mapping

class InMemoryObjectStore(ObjectStore):
  def __init__(self, object_store):
    self._object_store = object_store
    self._cache = {}

  def SetMulti(self, mapping, time=CACHE_TIMEOUT):
    for k, v in mapping.iteritems():
      self._cache[k] = _CacheEntry(v, time)
      # TODO(cduvall): Use a batch set? App Engine kept throwing:
      # ValueError: Values may not be more than 1000000 bytes in length
      # for the batch set.
      self._object_store.Set(k, v, time=time)

  def GetMulti(self, keys, time=CACHE_TIMEOUT):
    keys = keys[:]
    mapping = {}
    for key in keys:
      cache_entry = self._cache.get(key, None)
      if cache_entry is None or cache_entry.HasExpired():
        mapping[key] = None
      else:
        mapping[key] = cache_entry.value
        keys.remove(key)
    future = self._object_store.GetMulti(keys, time=time)
    return Future(delegate=_AsyncGetFuture(self._cache,
                                           time,
                                           future,
                                           mapping))

  def Delete(self, key):
    if key in self._cache:
      self._cache.pop(key)
    self._object_store.Delete(key)
