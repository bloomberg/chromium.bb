# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from caching_file_system import CachingFileSystem
from local_file_system import LocalFileSystem
from offline_file_system import OfflineFileSystem
from subversion_file_system import SubversionFileSystem

class HostFileSystemCreator(object):
  '''Creates host file systems with configuration information. By default, SVN
  file systems are created, although a constructor method can be passed in to
  override this behavior (likely for testing purposes).
  '''
  def __init__(self,
               object_store_creator,
               offline=False,
               constructor_for_test=None):
     self._object_store_creator = object_store_creator
     # Determines whether or not created file systems will be wrapped in an
     # OfflineFileSystem.
     self._offline = offline
     # Provides custom create behavior, useful in tests.
     self._constructor_for_test = constructor_for_test

  def Create(self, branch='trunk', revision=None, offline=None):
    '''Creates either SVN file systems or specialized file systems from the
    constructor passed into this instance. Wraps the resulting file system in
    an Offline file system if the offline flag is set, and finally wraps it in a
    Caching file system.
    '''
    if self._constructor_for_test is not None:
      file_system = self._constructor_for_test(branch=branch, revision=revision)
    else:
      file_system = SubversionFileSystem.Create(branch=branch,
                                                revision=revision)
    if offline or (offline is None and self._offline):
      file_system = OfflineFileSystem(file_system)
    return CachingFileSystem(file_system, self._object_store_creator)

  @staticmethod
  def ForLocal(object_store_creator):
    '''Used in creating a server instance on localhost.
    '''
    return HostFileSystemCreator(
        object_store_creator,
        constructor_for_test=lambda **_: LocalFileSystem.Create())

  @staticmethod
  def ForTest(file_system, object_store_creator):
    '''Used in creating a test server instance. The HostFileSystemCreator
    returned here will always return |file_system| when its Create() method is
    called.
    '''
    return HostFileSystemCreator(
        object_store_creator,
        constructor_for_test=lambda **_: file_system)
