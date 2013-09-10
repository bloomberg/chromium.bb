// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_LOCAL_SYNC_OPERATION_RESOLVER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_LOCAL_SYNC_OPERATION_RESOLVER_H_

#include "base/gtest_prod_util.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/sync_file_type.h"
#include "chrome/browser/sync_file_system/sync_operation_type.h"

namespace sync_file_system {

class DriveMetadata;
class FileChange;

class LocalSyncOperationResolver {
 public:
  // |remote_file_change| is non-null when we have a remote change for the file,
  // and |drive_metadata| is also non-null when we have metadata.
  static SyncOperationType Resolve(
      const FileChange& local_file_change,
      const FileChange* remote_file_change,
      const DriveMetadata* drive_metadata);

 private:
  static SyncOperationType ResolveForAddOrUpdateFile(
      const FileChange* remote_file_change,
      SyncFileType remote_file_type_in_metadata);
  static SyncOperationType ResolveForAddOrUpdateFileInConflict(
      const FileChange* remote_file_change);
  static SyncOperationType ResolveForAddDirectory(
      const FileChange* remote_file_change,
      SyncFileType remote_file_type_in_metadata);
  static SyncOperationType ResolveForAddDirectoryInConflict();
  static SyncOperationType ResolveForDelete(
      const FileChange* remote_file_change,
      SyncFileType remote_file_type_in_metadata);
  static SyncOperationType ResolveForDeleteInConflict(
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
                           ResolveForDelete);
  FRIEND_TEST_ALL_PREFIXES(LocalSyncOperationResolverTest,
                           ResolveForDeleteInConflict);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_V1_LOCAL_SYNC_OPERATION_RESOLVER_H_
