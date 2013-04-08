# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from appengine_wrappers import CACHE_TIMEOUT
from future import Future

class _SingleGetFuture(object):
  def __init__(self, multi_get, key):
    self._future = multi_get
    self._key = key

  def Get(self):
    return self._future.Get().get(self._key)

class ObjectStore(object):
  """A class for caching picklable objects.
  """
  def Set(self, key, value, time=CACHE_TIMEOUT):
    """Sets key -> value in the object store, with the specified timeout.
    """
    self.SetMulti({ key: value }, time=time)

  def SetMulti(self, mapping, time=CACHE_TIMEOUT):
    """Sets the mapping of keys to values in the object store with the specified
    timeout.
    """
    raise NotImplementedError()

  def Get(self, key, time=CACHE_TIMEOUT):
    """Gets a |Future| with the value of |key| in the object store, or None
    if |key| is not in the object store.
    """
    return Future(delegate=_SingleGetFuture(self.GetMulti([key], time=time),
                                            key))

  def GetMulti(self, keys, time=CACHE_TIMEOUT):
    """Gets a |Future| with values mapped to |keys| from the object store, with
    any keys not in the object store mapped to None.
    """
    raise NotImplementedError()

  def Delete(self, key):
    """Deletes a key from the object store.
    """
    raise NotImplementedError()
