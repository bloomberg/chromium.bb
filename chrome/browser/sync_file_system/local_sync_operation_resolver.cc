// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local_sync_operation_resolver.h"

#include "base/logging.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"

namespace sync_file_system {

LocalSyncOperationType LocalSyncOperationResolver::Resolve(
    const FileChange& local_file_change,
    bool has_remote_change,
    const FileChange& remote_file_change,
    DriveMetadata* drive_metadata) {
  // Invalid combination should never happen.
  if (has_remote_change && remote_file_change.IsTypeUnknown())
    return LOCAL_SYNC_OPERATION_FAIL;

  bool is_conflicting = false;
  SyncFileType remote_file_type_in_metadata = SYNC_FILE_TYPE_UNKNOWN;
  if (drive_metadata != NULL) {
    is_conflicting = drive_metadata->conflicted();
    if (drive_metadata->type() ==
        DriveMetadata_ResourceType_RESOURCE_TYPE_FILE)
      remote_file_type_in_metadata = SYNC_FILE_TYPE_FILE;
    else
      remote_file_type_in_metadata = SYNC_FILE_TYPE_DIRECTORY;
  }

  switch (local_file_change.change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      switch (local_file_change.file_type()) {
        case SYNC_FILE_TYPE_FILE:
          return is_conflicting
              ? ResolveForAddOrUpdateFileInConflict(has_remote_change,
                                                    remote_file_change)
              : ResolveForAddOrUpdateFile(has_remote_change,
                                          remote_file_change,
                                          remote_file_type_in_metadata);
        case SYNC_FILE_TYPE_DIRECTORY:
          return is_conflicting
              ? ResolveForAddDirectoryInConflict(has_remote_change,
                                                 remote_file_change)
              : ResolveForAddDirectory(has_remote_change,
                                       remote_file_change,
                                       remote_file_type_in_metadata);
        case SYNC_FILE_TYPE_UNKNOWN:
          NOTREACHED();
          return LOCAL_SYNC_OPERATION_FAIL;
      }
    case FileChange::FILE_CHANGE_DELETE:
      switch (local_file_change.file_type()) {
        case SYNC_FILE_TYPE_FILE:
          return is_conflicting
              ? ResolveForDeleteFileInConflict(has_remote_change,
                                               remote_file_change)
              : ResolveForDeleteFile(has_remote_change,
                                     remote_file_change,
                                     remote_file_type_in_metadata);
        case SYNC_FILE_TYPE_DIRECTORY:
          return is_conflicting
              ? ResolveForDeleteDirectoryInConflict(has_remote_change,
                                                    remote_file_change)
              : ResolveForDeleteDirectory(has_remote_change,
                                          remote_file_change,
                                          remote_file_type_in_metadata);
        case SYNC_FILE_TYPE_UNKNOWN:
          NOTREACHED();
          return LOCAL_SYNC_OPERATION_FAIL;
      }
  }
  NOTREACHED();
  return LOCAL_SYNC_OPERATION_FAIL;
}

LocalSyncOperationType LocalSyncOperationResolver::ResolveForAddOrUpdateFile(
    bool has_remote_change,
    const FileChange& remote_file_change,
    SyncFileType remote_file_type_in_metadata) {
  if (!has_remote_change) {
    switch (remote_file_type_in_metadata) {
      case SYNC_FILE_TYPE_UNKNOWN:
        // Remote file or directory may not exist.
        return LOCAL_SYNC_OPERATION_ADD_FILE;
      case SYNC_FILE_TYPE_FILE:
        return LOCAL_SYNC_OPERATION_UPDATE_FILE;
      case SYNC_FILE_TYPE_DIRECTORY:
        return LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE;
    }
  }

  switch (remote_file_change.change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      if (remote_file_change.IsFile())
        return LOCAL_SYNC_OPERATION_CONFLICT;
      return LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE;
    case FileChange::FILE_CHANGE_DELETE:
      return LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL;
  }

  NOTREACHED();
  return LOCAL_SYNC_OPERATION_FAIL;
}

LocalSyncOperationType
LocalSyncOperationResolver::ResolveForAddOrUpdateFileInConflict(
    bool has_remote_change,
    const FileChange& remote_file_change) {
  if (!has_remote_change)
    return LOCAL_SYNC_OPERATION_CONFLICT;
  switch (remote_file_change.change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      if (remote_file_change.IsFile())
        return LOCAL_SYNC_OPERATION_CONFLICT;
      return LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE;
    case FileChange::FILE_CHANGE_DELETE:
      return LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL;
  }
  NOTREACHED();
  return LOCAL_SYNC_OPERATION_FAIL;
}

LocalSyncOperationType LocalSyncOperationResolver::ResolveForAddDirectory(
    bool has_remote_change,
    const FileChange& remote_file_change,
    SyncFileType remote_file_type_in_metadata) {
  if (!has_remote_change) {
    switch (remote_file_type_in_metadata) {
      case SYNC_FILE_TYPE_UNKNOWN:
        // Remote file or directory may not exist.
        return LOCAL_SYNC_OPERATION_ADD_DIRECTORY;
      case SYNC_FILE_TYPE_FILE:
        return LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL;
      case SYNC_FILE_TYPE_DIRECTORY:
        return LOCAL_SYNC_OPERATION_NONE;
    }
  }

  switch (remote_file_change.change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      if (remote_file_change.IsFile())
        return LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL;
      return LOCAL_SYNC_OPERATION_NONE;
    case FileChange::FILE_CHANGE_DELETE:
      if (remote_file_change.IsFile())
        return LOCAL_SYNC_OPERATION_ADD_DIRECTORY;
      return LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL;
  }

  NOTREACHED();
  return LOCAL_SYNC_OPERATION_FAIL;
}

LocalSyncOperationType
LocalSyncOperationResolver::ResolveForAddDirectoryInConflict(
    bool has_remote_change,
    const FileChange& remote_file_change) {
  return LOCAL_SYNC_OPERATION_RESOLVE_TO_LOCAL;
}

LocalSyncOperationType LocalSyncOperationResolver::ResolveForDeleteFile(
    bool has_remote_change,
    const FileChange& remote_file_change,
    SyncFileType remote_file_type_in_metadata) {
  if (!has_remote_change) {
    switch (remote_file_type_in_metadata) {
      case SYNC_FILE_TYPE_UNKNOWN:
        // Remote file or directory may not exist.
        return LOCAL_SYNC_OPERATION_NONE;
      case SYNC_FILE_TYPE_FILE:
        return LOCAL_SYNC_OPERATION_DELETE_FILE;
      case SYNC_FILE_TYPE_DIRECTORY:
        return LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE;
    }
  }

  switch (remote_file_change.change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      return LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE;
    case FileChange::FILE_CHANGE_DELETE:
      return LOCAL_SYNC_OPERATION_DELETE_METADATA;
  }

  NOTREACHED();
  return LOCAL_SYNC_OPERATION_FAIL;
}

LocalSyncOperationType
LocalSyncOperationResolver::ResolveForDeleteFileInConflict(
    bool has_remote_change,
    const FileChange& remote_file_change) {
  if (!has_remote_change)
    return LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE;
  switch (remote_file_change.change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      return LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE;
    case FileChange::FILE_CHANGE_DELETE:
      return LOCAL_SYNC_OPERATION_DELETE_METADATA;
  }
  NOTREACHED();
  return LOCAL_SYNC_OPERATION_FAIL;
}

LocalSyncOperationType LocalSyncOperationResolver::ResolveForDeleteDirectory(
    bool has_remote_change,
    const FileChange& remote_file_change,
    SyncFileType remote_file_type_in_metadata) {
  if (!has_remote_change) {
    switch (remote_file_type_in_metadata) {
      case SYNC_FILE_TYPE_UNKNOWN:
        // Remote file or dircetory may not exist.
        return LOCAL_SYNC_OPERATION_NONE;
      case SYNC_FILE_TYPE_FILE:
        return LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE;
      case SYNC_FILE_TYPE_DIRECTORY:
        return LOCAL_SYNC_OPERATION_DELETE_DIRECTORY;
    }
  }

  switch (remote_file_change.change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      return LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE;
    case FileChange::FILE_CHANGE_DELETE:
      return LOCAL_SYNC_OPERATION_DELETE_METADATA;
  }

  NOTREACHED();
  return LOCAL_SYNC_OPERATION_FAIL;
}

LocalSyncOperationType
LocalSyncOperationResolver::ResolveForDeleteDirectoryInConflict(
    bool has_remote_change,
    const FileChange& remote_file_change) {
  if (!has_remote_change)
    return LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE;
  switch (remote_file_change.change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      return LOCAL_SYNC_OPERATION_RESOLVE_TO_REMOTE;
    case FileChange::FILE_CHANGE_DELETE:
      return LOCAL_SYNC_OPERATION_DELETE_METADATA;
  }
  NOTREACHED();
  return LOCAL_SYNC_OPERATION_FAIL;
}

}  // namespace sync_file_system
