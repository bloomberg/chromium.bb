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
  # Default to a 5 minute cache timeout.
  CACHE_TIMEOUT = 300
except ImportError:
  # Cache for one second because zero means cache forever.
  CACHE_TIMEOUT = 1
  import re
  from StringIO import StringIO

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
    class DownloadError(Exception):
      pass

    class _Response(object):
      def __init__(self, content):
        self.content = content
        self.headers = { 'content-type': 'none' }
        self.status_code = 200

    def fetch(self, url, **kwargs):
      response = self._Response(_GetConfiguration(url).fetch(url))
      if response.content is None:
        response.status_code = 404
      return response

    def create_rpc(self):
      return _RPC()

    def make_fetch_call(self, rpc, url, **kwargs):
      rpc.result = self.fetch(url)
  urlfetch = FakeUrlFetch()

  class NotImplemented(object):
    def __getattr__(self, attr):
      raise NotImplementedError()

  _BLOBS = {}
  class FakeBlobstore(object):
    class BlobReader(object):
      def __init__(self, blob_key):
        self._data = _BLOBS[blob_key].getvalue()

      def read(self):
        return self._data

  blobstore = FakeBlobstore()

  class FakeFileInterface(object):
    """This class allows a StringIO object to be used in a with block like a
    file.
    """
    def __init__(self, io):
      self._io = io

    def __exit__(self, *args):
      pass

    def write(self, data):
      self._io.write(data)

    def __enter__(self, *args):
      return self._io

  class FakeFiles(object):
    _next_blobstore_key = 0
    class blobstore(object):
      @staticmethod
      def create():
        FakeFiles._next_blobstore_key += 1
        return FakeFiles._next_blobstore_key

      @staticmethod
      def get_blob_key(filename):
        return filename

    def open(self, filename, mode):
      _BLOBS[filename] = StringIO()
      return FakeFileInterface(_BLOBS[filename])

    def GetBlobKeys(self):
      return _BLOBS.keys()

    def finalize(self, filename):
      pass

  files = FakeFiles()

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
        self.response.status = 200

      def redirect(self, path, permanent=False):
        self.response.status = 301 if permanent else 302
        self.response.headers['Location'] = path

  class _Db_Result(object):
    def __init__(self, data):
      self._data = data

    class _Result(object):
      def __init__(self, value):
        self.value = value

    def get(self):
      return self._Result(self._data)

  class db(object):
    _store = {}
    class StringProperty(object):
      pass

    class Model(object):
      def __init__(self, key_='', value=''):
        self._key = key_
        self._value = value

      @staticmethod
      def gql(query, key):
        return _Db_Result(db._store.get(key, None))

      def put(self):
        db._store[self._key] = self._value

  class BlobReferenceProperty(object):
    pass
