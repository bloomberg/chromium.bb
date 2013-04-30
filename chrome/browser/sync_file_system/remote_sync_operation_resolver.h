// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef  CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_SYNC_OPERATION_RESOLVER_H_
#define  CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_SYNC_OPERATION_RESOLVER_H_

#include "base/gtest_prod_util.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/sync_file_type.h"

namespace sync_file_system {

enum RemoteSyncOperationType {
  REMOTE_SYNC_OPERATION_ADD_FILE,
  REMOTE_SYNC_OPERATION_ADD_DIRECTORY,
  REMOTE_SYNC_OPERATION_UPDATE_FILE,
  REMOTE_SYNC_OPERATION_DELETE_FILE,
  REMOTE_SYNC_OPERATION_DELETE_DIRECTORY,
  REMOTE_SYNC_OPERATION_NONE,
  REMOTE_SYNC_OPERATION_CONFLICT,
  REMOTE_SYNC_OPERATION_RESOLVE_TO_LOCAL,
  REMOTE_SYNC_OPERATION_RESOLVE_TO_REMOTE,
  REMOTE_SYNC_OPERATION_DELETE_METADATA,
  REMOTE_SYNC_OPERATION_FAIL,
};

class RemoteSyncOperationResolver {
 public:
  static RemoteSyncOperationType Resolve(
      const FileChange& remote_file_change,
      const FileChangeList& local_changes,
      SyncFileType local_file_type,
      bool is_conflicting);

 private:
  static RemoteSyncOperationType ResolveForAddOrUpdateFile(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static RemoteSyncOperationType ResolveForAddOrUpdateFileInConflict(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static RemoteSyncOperationType ResolveForAddDirectory(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static RemoteSyncOperationType ResolveForAddDirectoryInConflict(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static RemoteSyncOperationType ResolveForDeleteFile(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static RemoteSyncOperationType ResolveForDeleteFileInConflict(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static RemoteSyncOperationType ResolveForDeleteDirectory(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static RemoteSyncOperationType ResolveForDeleteDirectoryInConflict(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);

  FRIEND_TEST_ALL_PREFIXES(RemoteSyncOperationResolverTest,
                           ResolveForAddOrUpdateFile);
  FRIEND_TEST_ALL_PREFIXES(RemoteSyncOperationResolverTest,
                           ResolveForAddOrUpdateFileInConflict);
  FRIEND_TEST_ALL_PREFIXES(RemoteSyncOperationResolverTest,
                           ResolveForAddDirectory);
  FRIEND_TEST_ALL_PREFIXES(RemoteSyncOperationResolverTest,
                           ResolveForAddDirectoryInConflict);
  FRIEND_TEST_ALL_PREFIXES(RemoteSyncOperationResolverTest,
                           ResolveForDeleteFile);
  FRIEND_TEST_ALL_PREFIXES(RemoteSyncOperationResolverTest,
                           ResolveForDeleteFileInConflict);
  FRIEND_TEST_ALL_PREFIXES(RemoteSyncOperationResolverTest,
                           ResolveForDeleteDirectory);
  FRIEND_TEST_ALL_PREFIXES(RemoteSyncOperationResolverTest,
                           ResolveForDeleteDirectoryInConflict);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_REMOTE_SYNC_OPERATION_RESOLVER_H_
