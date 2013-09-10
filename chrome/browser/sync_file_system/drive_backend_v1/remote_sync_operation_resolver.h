// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_REMOTE_SYNC_OPERATION_RESOLVER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_REMOTE_SYNC_OPERATION_RESOLVER_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/sync_file_type.h"
#include "chrome/browser/sync_file_system/sync_operation_type.h"

namespace sync_file_system {

class RemoteSyncOperationResolver {
 public:
  static SyncOperationType Resolve(
      const FileChange& remote_file_change,
      const FileChangeList& local_changes,
      SyncFileType local_file_type,
      bool is_conflicting);

 private:
  static SyncOperationType ResolveForAddOrUpdateFile(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static SyncOperationType ResolveForAddOrUpdateFileInConflict(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static SyncOperationType ResolveForAddDirectory(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static SyncOperationType ResolveForAddDirectoryInConflict(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static SyncOperationType ResolveForDeleteFile(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static SyncOperationType ResolveForDeleteFileInConflict(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static SyncOperationType ResolveForDeleteDirectory(
      const FileChangeList& local_changes,
      SyncFileType local_file_type);
  static SyncOperationType ResolveForDeleteDirectoryInConflict(
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

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_REMOTE_SYNC_OPERATION_RESOLVER_H_
