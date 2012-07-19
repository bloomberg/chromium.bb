# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class InMemoryMemcache(object):
  """A memcache that stores items in memory instead of using the memcache
  module.
  """
  def __init__(self):
    self._cache = {}

  def Set(self, key, value, namespace, time=60):
    if namespace not in self._cache:
      self._cache[namespace] = {}
    self._cache[namespace][key] = value

  def Get(self, key, namespace):
    if namespace not in self._cache:
      return None
    return self._cache[namespace].get(key, None)

  def Delete(self, key, namespace):
    if namespace in self._cache:
      self._cache[namespace].pop(key)
