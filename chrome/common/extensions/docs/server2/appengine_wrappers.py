# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This will attempt to import the actual App Engine modules, and if it fails,
# they will be replaced with fake modules. This is useful during testing.
try:
  import google.appengine.ext.webapp as webapp
  import google.appengine.api.memcache as memcache
  import google.appengine.api.urlfetch as urlfetch
except ImportError:
  class FakeUrlFetch(object):
    """A fake urlfetch module that errors for all method calls.
    """
    def fetch(self, url):
      raise NotImplementedError()

    def create_rpc(self):
      raise NotImplementedError()

    def make_fetch_call(self):
      raise NotImplementedError()
  urlfetch = FakeUrlFetch()

  class InMemoryMemcache(object):
    """A memcache that stores items in memory instead of using the memcache
    module.
    """
    def __init__(self):
      self._cache = {}

    def set(self, key, value, namespace, time=60):
      if namespace not in self._cache:
        self._cache[namespace] = {}
      self._cache[namespace][key] = value

    def get(self, key, namespace):
      if namespace not in self._cache:
        return None
      return self._cache[namespace].get(key, None)

    def delete(self, key, namespace):
      if namespace in self._cache:
        self._cache[namespace].pop(key)
  memcache = InMemoryMemcache()

  # A fake webapp.RequestHandler class for Handler to extend.
  class webapp(object):
    class RequestHandler(object):
      def __init__(self, request, response):
        self.request = request
        self.response = response

      def redirect(self, path):
        self.request.path = path
