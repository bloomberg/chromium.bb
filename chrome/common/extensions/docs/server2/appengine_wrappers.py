# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from in_memory_memcache import InMemoryMemcache

# This will attempt to import the actual Appengine modules, and if it fails,
# they will be replaced with fake modules. This is useful during testing.
try:
  import google.appengine.ext.webapp as webapp
  import google.appengine.api.memcache as memcache
  import google.appengine.api.urlfetch as urlfetch
except ImportError:
  class FakeUrlFetch(object):
    def fetch(self, url):
      raise NotImplementedError()

    def create_rpc(self):
      raise NotImplementedError()

    def make_fetch_call(self):
      raise NotImplementedError()
  urlfetch = FakeUrlFetch()

  # Use an in-memory memcache if Appengine memcache isn't available.
  memcache = InMemoryMemcache()

  # A fake webapp.RequestHandler class for Handler to extend.
  class webapp(object):
    class RequestHandler(object):
      def __init__(self, request, response):
        self.request = request
        self.response = response
