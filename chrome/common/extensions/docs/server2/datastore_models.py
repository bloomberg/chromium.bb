# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from appengine_wrappers import db
import cPickle

# A collection of the data store models used throughout the server.
# These values are global within datastore.

class PersistentObjectStoreItem(db.Model):
  pickled_value = db.BlobProperty()

  @classmethod
  def CreateKey(cls, namespace, key):
    return db.Key.from_path(cls.__name__, '%s/%s' % (namespace, key))

  @classmethod
  def CreateItem(cls, namespace, key, value):
    return PersistentObjectStoreItem(key=cls.CreateKey(namespace, key),
                                     pickled_value=cPickle.dumps(value))

  def GetValue(self):
    return cPickle.loads(self.pickled_value)
