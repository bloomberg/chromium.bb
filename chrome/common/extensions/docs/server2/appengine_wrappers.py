# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

def IsDeadlineExceededError(error):
  '''A general way of determining whether |error| is a DeadlineExceededError,
  since there are 3 different types thrown by AppEngine and we might as well
  handle them all the same way. For more info see:
  https://developers.google.com/appengine/articles/deadlineexceedederrors
  '''
  return type(error).__name__ == 'DeadlineExceededError'


def IsDownloadError(error):
  return type(error).__name__ == 'DownloadError'


# This will attempt to import the actual App Engine modules, and if it fails,
# they will be replaced with fake modules. This is useful during testing.
try:
  import google.appengine.api.app_identity as app_identity
  import google.appengine.api.files as files
  import google.appengine.api.logservice as logservice
  import google.appengine.api.memcache as memcache
  import google.appengine.api.taskqueue as taskqueue
  import google.appengine.api.urlfetch as urlfetch
  import google.appengine.ext.blobstore as blobstore
  from google.appengine.ext.blobstore.blobstore import BlobReferenceProperty
  import google.appengine.ext.db as db
  import webapp2
except ImportError:
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
    raise ValueError('No configuration found for %s' % key)

  class _RPC(object):
    def __init__(self, result=None):
      self.result = result

    def get_result(self):
      return self.result

    def wait(self):
      pass

  class FakeAppIdentity(object):
    """A fake app_identity module that returns no access tokens."""
    def get_access_token(self, scope):
      return (None, None)
  app_identity = FakeAppIdentity()

  class FakeUrlFetch(object):
    """A fake urlfetch module that uses the current
    |FAKE_URL_FETCHER_CONFIGURATION| to map urls to fake fetchers.
    """
    class DownloadError(Exception):
      pass

    class _Response(object):
      def __init__(self, content):
        self.content = content
        self.headers = {'Content-Type': 'none'}
        self.status_code = 200

    def fetch(self, url, **kwargs):
      url = url.split('?', 1)[0]
      response = self._Response(_GetConfiguration(url).fetch(url))
      if response.content is None:
        response.status_code = 404
      return response

    def create_rpc(self, **kwargs):
      return _RPC()

    def make_fetch_call(self, rpc, url, **kwargs):
      rpc.result = self.fetch(url)
  urlfetch = FakeUrlFetch()

  _BLOBS = {}
  class FakeBlobstore(object):
    class BlobNotFoundError(Exception):
      pass

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

  class Logservice(object):
    AUTOFLUSH_ENABLED = True

    def flush(self):
      pass

  logservice = Logservice()

  class InMemoryMemcache(object):
    """An in-memory memcache implementation.
    """
    def __init__(self):
      self._namespaces = {}

    class Client(object):
      def set_multi_async(self, mapping, namespace='', time=0):
        for k, v in mapping.iteritems():
          memcache.set(k, v, namespace=namespace, time=time)

      def get_multi_async(self, keys, namespace='', time=0):
        return _RPC(result=dict(
          (k, memcache.get(k, namespace=namespace, time=time)) for k in keys))

    def set(self, key, value, namespace='', time=0):
      self._GetNamespace(namespace)[key] = value

    def get(self, key, namespace='', time=0):
      return self._GetNamespace(namespace).get(key)

    def delete(self, key, namespace=''):
      self._GetNamespace(namespace).pop(key, None)

    def delete_multi(self, keys, namespace=''):
      for k in keys:
        self.delete(k, namespace=namespace)

    def _GetNamespace(self, namespace):
      if namespace not in self._namespaces:
        self._namespaces[namespace] = {}
      return self._namespaces[namespace]

  memcache = InMemoryMemcache()

  class webapp2(object):
    class RequestHandler(object):
      """A fake webapp2.RequestHandler class for Handler to extend.
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

    class BlobProperty(object):
      pass

    class Key(object):
      def __init__(self, key):
        self._key = key

      @staticmethod
      def from_path(model_name, path):
        return db.Key('%s/%s' % (model_name, path))

      def __eq__(self, obj):
        return self.__class__ == obj.__class__ and self._key == obj._key

      def __hash__(self):
        return hash(self._key)

      def __str__(self):
        return str(self._key)

    class Model(object):
      key = None

      def __init__(self, **optargs):
        cls = self.__class__
        for k, v in optargs.iteritems():
          assert hasattr(cls, k), '%s does not define property %s' % (
              cls.__name__, k)
          setattr(self, k, v)

      @staticmethod
      def gql(query, key):
        return _Db_Result(db._store.get(key))

      def put(self):
        db._store[self.key_] = self.value

    @staticmethod
    def get_async(key):
      return _RPC(result=db._store.get(key))

    @staticmethod
    def delete_async(key):
      db._store.pop(key, None)
      return _RPC()

    @staticmethod
    def put_async(value):
      db._store[value.key] = value
      return _RPC()

  class BlobReferenceProperty(object):
    pass

  # Executes any queued tasks synchronously as they are queued.
  _task_runner = None

  def SetTaskRunnerForTest(task_runner):
    global _task_runner
    _task_runner = task_runner

  class SynchronousTaskQueue(object):
    class Task(object):
      def __init__(self, url=None, params={}):
        self.url_ = url
        self.params_ = params

      def GetUrl(self):
        return self.url_

      def GetCommit(self):
        return self.params_.get('commit')

    class Queue(object):
      def __init__(self, name='default'):
        pass

      def add(self, task):
        global _task_runner
        if _task_runner:
          _task_runner(task.GetUrl(), task.GetCommit())
        return _RPC()

      def purge(self):
        return _RPC()

  taskqueue = SynchronousTaskQueue()
