# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from cache_chain_object_store import CacheChainObjectStore
from memcache_object_store import MemcacheObjectStore
from persistent_object_store import PersistentObjectStore

class ObjectStoreCreator(object):
  class Factory(object):
    def __init__(self, branch=None):
      self._branch = branch

    '''Creates ObjectStoreCreators (yes seriously) bound to an SVN branch.
    '''
    def Create(self, cls, store_type=None):
      return ObjectStoreCreator(cls, branch=self._branch, store_type=store_type)

  def __init__(self, cls, branch=None, store_type=None):
    '''Creates stores with a top-level namespace given by the name of |cls|
    combined with |branch|. Set an explicit |store_type| if necessary for tests.

    By convention this should be the name of the class which owns the object
    store. If a class needs multiple object store it should use Create with the
    |category| argument.
    '''
    assert isinstance(cls, type)
    assert not cls.__name__[0].islower()  # guard against non-class types
    if branch is None:
      self._name = cls.__name__
    else:
      self._name = '%s@%s' % (cls.__name__, branch)
    self._store_type = store_type

  def Create(self, version=None, category=None):
    '''Creates a new object store with the top namespace given in the
    constructor, at version |version|, with an optional |category| for classes
    that need multiple object stores (e.g. one for stat and one for read).
    '''
    namespace = self._name
    if category is not None:
      assert not any(c.isdigit() for c in category)
      namespace = '%s/%s' % (namespace, category)
    if version is not None:
      assert isinstance(version, int)
      namespace = '%s/%s' % (namespace, version)
    if self._store_type is not None:
      return self._store_type(namespace)
    return CacheChainObjectStore((MemcacheObjectStore(namespace),
                                  PersistentObjectStore(namespace)))
