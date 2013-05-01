# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from cache_chain_object_store import CacheChainObjectStore
from memcache_object_store import MemcacheObjectStore
from test_object_store import TestObjectStore
from persistent_object_store import PersistentObjectStore

class ObjectStoreCreator(object):
  class Factory(object):
    '''Parameters:
    - |branch| The branch to create object stores for. This becomes part of the
      namespace for the object stores that are created.
    - |start_empty| Whether the caching object store that gets created should
      start empty, or start with the content of its delegate object stores.
    '''
    def __init__(self, app_version, branch):
      self._app_version = app_version
      self._branch = branch

    def Create(self, cls, store_type=None):
      return ObjectStoreCreator(cls,
                                self._app_version,
                                self._branch,
                                store_type=store_type)

  class SharedFactory(object):
    '''A |Factory| for creating object stores shared across branches.
    '''
    def __init__(self, app_version):
      # TODO(kalman): Pass in (app_version, None) here.
      self._factory = ObjectStoreCreator.Factory(app_version, 'shared')

    def Create(self, cls, store_type=None):
      return self._factory.Create(cls, store_type=store_type)

  class GlobalFactory(object):
    '''A |Factory| for creating object stores shared across all branches and
    app versions.
    '''
    def __init__(self):
      # TODO(kalman): Pass in (None, None) here.
      self._factory = ObjectStoreCreator.Factory('all', 'shared')

    def Create(self, cls, store_type=None):
      return self._factory.Create(cls, store_type=store_type)

  class TestFactory(object):
    '''A |Factory| for creating object stores for tests, with fake defaults.
    '''
    def __init__(self,
                 # TODO(kalman): make these version=None and branch=None.
                 version='test-version',
                 branch='test-branch',
                 store_type=TestObjectStore):
      self._factory = ObjectStoreCreator.Factory(version, branch)
      self._store_type = store_type

    def Create(self, cls):
      return self._factory.Create(cls, store_type=self._store_type)

  def __init__(self, cls, app_version, branch, store_type=None):
    '''Creates stores with a top-level namespace given by the name of |cls|
    combined with |branch|. Set an explicit |store_type| if necessary for tests.

    By convention this should be the name of the class which owns the object
    store. If a class needs multiple object stores it should use Create with the
    |category| argument.
    '''
    assert isinstance(cls, type)
    assert not cls.__name__[0].islower()  # guard against non-class types
    self._name = '%s/%s@%s' % (app_version, cls.__name__, branch)
    self._store_type = store_type

  def Create(self, category=None, start_empty=False):
    '''Creates a new object store with the top namespace given in the
    constructor with an optional |category| for classes that need multiple
    object stores (e.g. one for stat and one for read).
    '''
    namespace = self._name
    if category is not None:
      assert not any(c.isdigit() for c in category)
      namespace = '%s/%s' % (namespace, category)
    if self._store_type is not None:
      return self._store_type(namespace, start_empty=start_empty)
    return CacheChainObjectStore(
        (MemcacheObjectStore(namespace), PersistentObjectStore(namespace)),
        start_empty=start_empty)
