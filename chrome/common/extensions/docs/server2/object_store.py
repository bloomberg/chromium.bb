# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from appengine_wrappers import CACHE_TIMEOUT
from future import Future

BRANCH_UTILITY            = 'BranchUtility'
FILE_SYSTEM_CACHE         = 'CompiledFileSystem'
FILE_SYSTEM_CACHE_LISTING = 'CompiledFileSystemListing'
FILE_SYSTEM_READ          = 'Read'
FILE_SYSTEM_STAT          = 'Stat'
GITHUB_STAT               = 'GithubStat'

class _SingleGetFuture(object):
  def __init__(self, multi_get, key):
    self._future = multi_get
    self._key = key

  def Get(self):
    return self._future.Get()[self._key]

class ObjectStore(object):
  """A class for caching picklable objects.
  """
  def Set(self, key, value, namespace, time=CACHE_TIMEOUT):
    """Sets key -> value in the object store, with the specified timeout.
    """
    self.SetMulti({ key: value }, namespace, time=time)

  def SetMulti(self, mapping, namespace, time=CACHE_TIMEOUT):
    """Sets the mapping of keys to values in the object store with the specified
    timeout.
    """
    raise NotImplementedError()

  def Get(self, key, namespace, time=CACHE_TIMEOUT):
    """Gets a |Future| with the value of |key| in the object store, or None
    if |key| is not in the object store.
    """
    return Future(delegate=_SingleGetFuture(
        self.GetMulti([key], namespace, time=time),
        key))

  def GetMulti(self, keys, namespace, time=CACHE_TIMEOUT):
    """Gets a |Future| with values mapped to |keys| from the object store, with
    any keys not in the object store mapped to None.
    """
    raise NotImplementedError()

  def Delete(self, key, namespace):
    """Deletes a key from the object store.
    """
    raise NotImplementedError()
