# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import time

class FetcherCache(object):
  """A cache for fetcher objects.
  """
  class Builder(object):
    """A class to build a fetcher cache.
    """
    def __init__(self, fetcher, timeout_seconds):
      self._fetcher = fetcher
      self._timeout_seconds = timeout_seconds

    def build(self, populate_function):
      return FetcherCache(self._fetcher,
                          self._timeout_seconds,
                          populate_function)

  class _CacheEntry(object):
    def __init__(self, cache_data, expiry):
      self._cache_data = cache_data
      self._expiry = expiry

    def HasExpired(self):
      return time.time() > self._expiry

  def __init__(self, fetcher, timeout_seconds, populate_function):
    self._fetcher = fetcher
    self._timeout_seconds = timeout_seconds
    self._populate_function = populate_function
    self._cache = {}

  def get(self, key):
    if key in self._cache:
      if self._cache[key].HasExpired():
        self._cache.pop(key)
      else:
        return self._cache[key]._cache_data
    cache_data = self._fetcher.FetchResource(key).content
    self._cache[key] = self._CacheEntry(self._populate_function(cache_data),
                                        time.time() + self._timeout_seconds)
    return self._cache[key]._cache_data
