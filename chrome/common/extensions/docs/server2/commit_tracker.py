# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from object_store_creator import ObjectStoreCreator
from future import Future


class CommitTracker(object):
  '''Utility class for managing and querying the storage of various named commit
  IDs.'''
  def __init__(self, object_store_creator):
    # The object store should never be created empty since the sole purpose of
    # this tracker is to persist named commit data across requests.
    self._store = object_store_creator.Create(CommitTracker, start_empty=False)

  def Get(self, key):
    return self._store.Get(key)

  def Set(self, key, commit):
    return self._store.Set(key, commit)
