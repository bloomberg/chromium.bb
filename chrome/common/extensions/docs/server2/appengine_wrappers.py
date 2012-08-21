# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This will attempt to import the actual App Engine modules, and if it fails,
# they will be replaced with fake modules. This is useful during testing.
try:
  import google.appengine.ext.blobstore as blobstore
  from google.appengine.ext.blobstore.blobstore import BlobReferenceProperty
  import google.appengine.ext.db as db
  import google.appengine.ext.webapp as webapp
  import google.appengine.api.files as files
  import google.appengine.api.memcache as memcache
  import google.appengine.api.urlfetch as urlfetch
except ImportError:
  import re

  FAKE_URL_FETCHER_CONFIGURATION = None

  def ConfigureFakeUrlFetch(configuration):
    """|configuration| is a dictionary mapping strings to fake urlfetch classes.
    A fake urlfetch class just needs to have a fetch method. The keys of the
    dictionary are treated as regex, and they are matched with the URL to
    determine which fake urlfetch is used.
    """
    global FAKE_URL_FETCHER_CONFIGURATION
    FAKE_URL_FETCHER_CONFIGURATION = dict(
        (re.compile(k), v) for k, v in configuration.iteritems())

  def _GetConfiguration(key):
    if not FAKE_URL_FETCHER_CONFIGURATION:
      raise ValueError('No fake fetch paths have been configured. '
                       'See ConfigureFakeUrlFetch in appengine_wrappers.py.')
    for k, v in FAKE_URL_FETCHER_CONFIGURATION.iteritems():
      if k.match(key):
        return v
    return None

  class _RPC(object):
    def __init__(self, result=None):
      self.result = result

    def get_result(self):
      return self.result

  class FakeUrlFetch(object):
    """A fake urlfetch module that uses the current
    |FAKE_URL_FETCHER_CONFIGURATION| to map urls to fake fetchers.
    """
    class _Response(object):
      def __init__(self, content):
        self.content = content
        self.headers = { 'content-type': 'none' }
        self.status_code = 200

    def fetch(self, url):
      return self._Response(_GetConfiguration(url).fetch(url))

    def create_rpc(self):
      return _RPC()

    def make_fetch_call(self, rpc, url):
      rpc.result = self.fetch(url)
  urlfetch = FakeUrlFetch()

  class NotImplemented(object):
    def __getattr__(self, attr):
      raise NotImplementedError()

  blobstore = NotImplemented()
  files = NotImplemented()

  class InMemoryMemcache(object):
    """A fake memcache that does nothing.
    """
    class Client(object):
      def set_multi_async(self, mapping, namespace='', time=0):
        return

      def get_multi_async(self, keys, namespace='', time=0):
        return _RPC(result=dict((k, None) for k in keys))

    def set(self, key, value, namespace='', time=0):
      return

    def get(self, key, namespace='', time=0):
      return None

    def delete(self, key, namespace):
      return
  memcache = InMemoryMemcache()

  class webapp(object):
    class RequestHandler(object):
      """A fake webapp.RequestHandler class for Handler to extend.
      """
      def __init__(self, request, response):
        self.request = request
        self.response = response

      def redirect(self, path):
        self.request.path = path

  class _Db_Result(object):
    def get(self):
      return []

  class db(object):
    class StringProperty(object):
      pass

    class Model(object):
      @staticmethod
      def gql(*args):
        return _Db_Result()

  class BlobReferenceProperty(object):
    pass
