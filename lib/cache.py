# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Contains on-disk caching functionality."""

import logging
import os
import shutil

from chromite.lib import cros_build_lib
from chromite.lib import locking
from chromite.lib import osutils

# pylint: disable=W0212

def EntryLock(f):
  """Decorator that provides monitor access control."""
  def new_f(self, *args, **kwargs):
    # Ensure we don't have a read lock before potentially blocking while trying
    # to access the monitor.
    if self.read_locked:
      raise AssertionError(
          'Cannot call %s while holding a read lock.' % f.__name__)

    with self._entry_lock:
      self._entry_lock.write_lock()
      return f(self, *args, **kwargs)
  return new_f


def WriteLock(f):
  """Decorator that takes a write lock."""
  def new_f(self, *args, **kwargs):
    with self._lock.write_lock():
      return f(self, *args, **kwargs)
  return new_f


class CacheReference(object):
  """Encapsulates operations on a cache key reference.

  CacheReferences are returned by the DiskCache.Lookup() function.  They are
  used to read from and insert into the cache.

  A typical example of using a CacheReference:

  @contextlib.contextmanager
  def FetchFromCache()
    with cache.Lookup(key) as ref:
       # If entry doesn't exist in cache already, generate it ourselves, and
       # insert it into the cache, acquiring a read lock on it in the process.
       # If the entry does exist, we grab a read lock on it.
      if not ref.Exists(lock=True):
        path = PrepareItem()
        ref.SetDefault(path, lock=True)

      # yield the path to the cached entry to consuming code.
      yield ref.path
  """

  def __init__(self, cache, key):
    self._cache = cache
    self.key = key
    self.acquired = False
    self.read_locked = False
    self._lock = cache._LockForKey(key)
    self._entry_lock = cache._LockForKey(key, suffix='.entry_lock')

  @property
  def path(self):
    """Returns on-disk path to the cached item."""
    return self._cache._GetKeyPath(self.key)

  def Acquire(self):
    """Prepare the cache reference for operation.

    This must be called (either explicitly or through entering a 'with'
    context) before calling any methods that acquire locks, or mutates
    reference.
    """
    if self.acquired:
      raise AssertionError(
          'Attempting to acquire an already acquired reference.')

    self.acquired = True
    self._lock.__enter__()

  def Release(self):
    """Release the cache reference.  Causes any held locks to be released."""
    if not self.acquired:
      raise AssertionError(
          'Attempting to release an unacquired reference.')

    self.acquired = False
    self._lock.__exit__(None, None, None)

  def __enter__(self):
    self.Acquire()
    return self

  def __exit__(self, *args):
    self.Release()

  def _ReadLock(self):
    self._lock.read_lock()
    self.read_locked = True

  @WriteLock
  def _Assign(self, path):
    self._cache._Insert(self.key, path)

  @WriteLock
  def _AssignText(self, text):
    self._cache._InsertText(self.key, text)

  @WriteLock
  def _Remove(self, key):
    self._cache._Remove(key)

  def _Exists(self):
    return self._cache._KeyExists(self.key)

  @EntryLock
  def Assign(self, path):
    """Insert a file or a directory into the cache at the referenced key."""
    self._Assign(path)

  @EntryLock
  def AssignText(self, text):
    """Create a file containing |text| and assign it to the key.

    Arguments:
      text: Can be a string or an iterable.
    """
    self._AssignText(text)

  @EntryLock
  def Remove(self, key):
    """Removes the key entry from the cache."""
    self._Remove(key)

  @EntryLock
  def Exists(self, lock=False):
    """Tests for existence of entry.

    Arguments:
      lock: If the entry exists, acquire and maintain a read lock on it.
    """
    if self._Exists():
      if lock:
        self._ReadLock()
      return True
    return False

  @EntryLock
  def SetDefault(self, default_path, lock=False):
    """Assigns default_path if the entry doesn't exist.

    Arguments:
      default_path: The path to assign if the entry doesn't exist.
      lock: Acquire and maintain a read lock on the entry.
    """
    if not self._Exists():
      self._Assign(default_path)
    if lock:
      self._ReadLock()

  def Unlock(self):
    """Release read lock on the reference."""
    self._lock.unlock()


class DiskCache(object):
  """Locked file system cache keyed by tuples.

  Key entries can be files or directories.  Access to the cache is provided
  through CacheReferences, which are retrieved by using the cache Lookup()
  method.
  """
  # TODO(rcui): Add LRU cleanup functionality.

  _STAGING_DIR = 'staging'

  def __init__(self, cache_dir):
    self._cache_dir = cache_dir
    self.staging_dir = os.path.join(cache_dir, self._STAGING_DIR)

    osutils.SafeMakedirs(self._cache_dir)
    osutils.SafeMakedirs(self.staging_dir)

  def _KeyExists(self, key):
    return os.path.exists(self._GetKeyPath(key))

  def _GetKeyPath(self, key):
    """Get the on-disk path of a key."""
    return os.path.join(self._cache_dir, '+'.join(key))

  def _LockForKey(self, key, suffix='.lock'):
    """Returns an unacquired lock associated with a key."""
    key_path = self._GetKeyPath(key)
    osutils.SafeMakedirs(os.path.dirname(key_path))
    lock_path = os.path.join(self._cache_dir, os.path.dirname(key_path),
                             os.path.basename(key_path) + suffix)
    return locking.FileLock(lock_path)

  def _TempDirContext(self):
    return osutils.TempDir(base_dir=self.staging_dir)

  def _Insert(self, key, path):
    """Insert a file or a directory into the cache at a given key."""
    self._Remove(key)
    key_path = self._GetKeyPath(key)
    osutils.SafeMakedirs(os.path.dirname(key_path))
    shutil.move(path, key_path)

  def _InsertText(self, key, text):
    """Inserts a file containing |text| into the cache."""
    with self._TempDirContext() as tempdir:
      file_path = os.path.join(tempdir, 'tempfile')
      osutils.WriteFile(file_path, text)
      self._Insert(key, file_path)

  def _Remove(self, key):
    """Remove a key from the cache."""
    if self._KeyExists(key):
      with self._TempDirContext() as tempdir:
        shutil.move(self._GetKeyPath(key), tempdir)

  def Lookup(self, key):
    """Get a reference to a given key."""
    return CacheReference(self, key)


def Untar(path, cwd, sudo=False):
  """Untar a tarball."""
  functor = cros_build_lib.SudoRunCommand if sudo else cros_build_lib.RunCommand
  functor(['tar', '-xpf', path], cwd=cwd, debug_level=logging.DEBUG)


class TarballCache(DiskCache):
  """Supports caching of extracted tarball contents."""

  def __init__(self, cache_dir):
    DiskCache.__init__(self, cache_dir)

  def _Insert(self, key, tarball_path):
    """Insert a tarball and its extracted contents into the cache."""
    with osutils.TempDir(base_dir=self.staging_dir) as tempdir:
      extract_path = os.path.join(tempdir, 'extract')
      os.mkdir(extract_path)
      Untar(tarball_path, extract_path)
      DiskCache._Insert(self, key, extract_path)
