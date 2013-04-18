# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from appengine_wrappers import db, IsDevServer
from datastore_models import PersistentObjectStoreItem
from future import Future
import logging
from object_store import ObjectStore

class _AsyncGetFuture(object):
  def __init__(self, object_store, keys):
    self._futures = dict(
        (k, db.get_async(
            PersistentObjectStoreItem.CreateKey(object_store._namespace, k)))
         for k in keys)

  def Get(self):
    return dict((key, future.get_result().GetValue())
                for key, future in self._futures.iteritems()
                if future.get_result() is not None)

class PersistentObjectStore(ObjectStore):
  '''Stores data persistently using the AppEngine Datastore API.
  '''
  def __init__(self, namespace):
    self._namespace = namespace

  def SetMulti(self, mapping):
    futures = []
    for key, value in mapping.items():
      futures.append(db.put_async(
          PersistentObjectStoreItem.CreateItem(self._namespace, key, value)))
    # If running the dev server, the futures don't complete until the server is
    # *quitting*. This is annoying. Flush now.
    if IsDevServer():
      [future.wait() for future in futures]

  def GetMulti(self, keys):
    return Future(delegate=_AsyncGetFuture(self, keys))

  def DelMulti(self, keys):
    futures = []
    for key in keys:
      futures.append(db.delete_async(
        PersistentObjectStoreItem.CreateKey(self._namespace, key)))
    # If running the dev server, the futures don't complete until the server is
    # *quitting*. This is annoying. Flush now.
    if IsDevServer():
      [future.wait() for future in futures]
