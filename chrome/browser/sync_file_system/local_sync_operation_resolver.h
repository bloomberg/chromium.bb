// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNC_OPERATION_RESOLVER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNC_OPERATION_RESOLVER_H_

#include "base/gtest_prod_util.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/sync_file_type.h"

namespace sync_file_system {

class DriveMetadata;
class FileChange;

enum LocalSyncOperationType {
  LOCAL_SYNC_OPERATION_ADD_FILE,
  LOCAL_SYNC_OPERATION_ADD_DIRECTORY,
  LOCAL_SYNC_OPERATION_UPDATE_FILE,
  LOCAL_SYNC_OPERATION_DELETE_FILE,
  LOCAL_SYNC_OPERATION_DELETE_DIRECTORY,
  LOCAL_SYNC_OPERATION_NONE,
  LOCAL_SYNC_OPERATION_CONFLICT,
  LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL,
  LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE,
  LOCAL_SYNC_OPERATION_DELETE_METADATA,
  LOCAL_SYNC_OPERATION_FAIL,
};

class LocalSyncOperationResolver {
 public:
  // |remote_file_change| is non-null when we have a remote change for the file,
  // and |drive_metadata| is also non-null when we have metadata.
  static LocalSyncOperationType Resolve(
      const FileChange& local_file_change,
      const FileChange* remote_file_change,
      const DriveMetadata* drive_metadata);

 private:
  static LocalSyncOperationType ResolveForAddOrUpdateFile(
      const FileChange* remote_file_change,
      SyncFileType remote_file_type_in_metadata);
  static LocalSyncOperationType ResolveForAddOrUpdateFileInConflict(
      const FileChange* remote_file_change);
  static LocalSyncOperationType ResolveForAddDirectory(
      const FileChange* remote_file_change,
      SyncFileType remote_file_type_in_metadata);
  static LocalSyncOperationType ResolveForAddDirectoryInConflict();
  static LocalSyncOperationType ResolveForDeleteFile(
      const FileChange* remote_file_change,
      SyncFileType remote_file_type_in_metadata);
  static LocalSyncOperationType ResolveForDeleteFileInConflict(
      const FileChange* remote_file_change);
  static LocalSyncOperationType ResolveForDeleteDirectory(
      const FileChange* remote_file_change,
      SyncFileType remote_file_type_in_metadata);
  static LocalSyncOperationType ResolveForDeleteDirectoryInConflict(
      const FileChange* remote_file_change);

  FRIEND_TEST_ALL_PREFIXES(LocalSyncOperationResolverTest,
                           ResolveForAddOrUpdateFile);
  FRIEND_TEST_ALL_PREFIXES(LocalSyncOperationResolverTest,
                           ResolveForAddOrUpdateFileInConflict);
  FRIEND_TEST_ALL_PREFIXES(LocalSyncOperationResolverTest,
                           ResolveForAddDirectory);
  FRIEND_TEST_ALL_PREFIXES(LocalSyncOperationResolverTest,
                           ResolveForAddDirectoryInConflict);
  FRIEND_TEST_ALL_PREFIXES(LocalSyncOperationResolverTest,
                           ResolveForDeleteFile);
  FRIEND_TEST_ALL_PREFIXES(LocalSyncOperationResolverTest,
                           ResolveForDeleteFileInConflict);
  FRIEND_TEST_ALL_PREFIXES(LocalSyncOperationResolverTest,
                           ResolveForDeleteDirectory);
  FRIEND_TEST_ALL_PREFIXES(LocalSyncOperationResolverTest,
                           ResolveForDeleteDirectoryInConflict);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_SYNC_OPERATION_RESOLVER_H_
