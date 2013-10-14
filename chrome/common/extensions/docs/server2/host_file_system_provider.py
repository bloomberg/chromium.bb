# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from caching_file_system import CachingFileSystem
from local_file_system import LocalFileSystem
from offline_file_system import OfflineFileSystem
from subversion_file_system import SubversionFileSystem
from third_party.json_schema_compiler.memoize import memoize


class HostFileSystemProvider(object):
  '''Provides host file systems ("host" meaning the file system that hosts the
  server's source code and templates) tracking trunk, or any branch.

  File system instances are memoized to maintain the in-memory caches across
  multiple callers.
  '''
  def __init__(self,
               object_store_creator,
               max_trunk_revision=None,
               default_trunk_instance=None,
               offline=False,
               constructor_for_test=None):
    '''
    |object_store_creator|
      Provides caches for file systems that need one.
    |max_trunk_revision|
      If not None, the maximum revision that a 'trunk' file system will be
      created at. If None, 'trunk' file systems will use HEAD.
    |default_trunk_instance|
      If not None, 'trunk' file systems provided by this class without a
      specific revision will return |default_trunk_instance| instead.
    |offline|
      If True all provided file systems will be wrapped in an OfflineFileSystem.
    |constructor_for_test|
      Provides a custom constructor rather than creating SubversionFileSystems.
    '''
    self._object_store_creator = object_store_creator
    self._max_trunk_revision = max_trunk_revision
    self._default_trunk_instance = default_trunk_instance
    self._offline = offline
    self._constructor_for_test = constructor_for_test

  @memoize
  def GetTrunk(self, revision=None):
    '''Gets a file system tracking 'trunk'. Use this method rather than
    GetBranch('trunk') because the behaviour is subtly different; 'trunk' can
    be pinned to a max revision (|max_trunk_revision| in constructor) and can
    have its default instance overridden (|default_trunk_instance| in
    constructor).

    |revision| if non-None determines a specific revision to pin the host file
    system at, though it will be ignored if it exceeds |max_trunk_revision|.
    If None then |revision| will track |max_trunk_revision| if is has been
    set, or just HEAD (which might change during server runtime!).
    '''
    if revision is None:
      if self._default_trunk_instance is not None:
        return self._default_trunk_instance
      return self._Create('trunk', revision=self._max_trunk_revision)
    if self._max_trunk_revision is not None:
      revision = min(revision, self._max_trunk_revision)
    return self._Create('trunk', revision=revision)

  @memoize
  def GetBranch(self, branch):
    '''Gets a file system tracking |branch|, for example '1150' - anything other
    than 'trunk', which must be constructed via the GetTrunk() method.

    Note: Unlike GetTrunk this function doesn't take a |revision| argument
    since we assume that branches hardly ever change, while trunk frequently
    changes.
    '''
    assert isinstance(branch, basestring), 'Branch %s must be a string' % branch
    assert branch != 'trunk', 'Cannot specify branch=\'trunk\', use GetTrunk()'
    return self._Create(branch)

  def _Create(self, branch, revision=None):
    '''Creates SVN file systems (or if in a test, potentially whatever
    |self._constructor_for_test specifies). Wraps the resulting file system in
    an Offline file system if the offline flag is set, and finally wraps it in
    a Caching file system.
    '''
    if self._constructor_for_test is not None:
      file_system = self._constructor_for_test(branch=branch, revision=revision)
    else:
      file_system = SubversionFileSystem.Create(branch=branch,
                                                revision=revision)
    if self._offline:
      file_system = OfflineFileSystem(file_system)
    return CachingFileSystem(file_system, self._object_store_creator)

  @staticmethod
  def ForLocal(object_store_creator, **optargs):
    '''Used in creating a server instance on localhost.
    '''
    return HostFileSystemProvider(
        object_store_creator,
        constructor_for_test=lambda **_: LocalFileSystem.Create(),
        **optargs)

  @staticmethod
  def ForTest(file_system, object_store_creator, **optargs):
    '''Used in creating a test server instance. The HostFileSystemProvider
    returned here will always return |file_system| when its Create() method is
    called.
    '''
    return HostFileSystemProvider(
        object_store_creator,
        constructor_for_test=lambda **_: file_system,
        **optargs)
