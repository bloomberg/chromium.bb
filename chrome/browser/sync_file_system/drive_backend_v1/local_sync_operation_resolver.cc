// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/local_sync_operation_resolver.h"

#include "base/logging.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system.pb.h"

namespace sync_file_system {

SyncOperationType LocalSyncOperationResolver::Resolve(
    const FileChange& local_file_change,
    const FileChange* remote_file_change,
    const DriveMetadata* drive_metadata) {
  // Invalid combination should never happen.
  if (remote_file_change && remote_file_change->IsTypeUnknown())
    return SYNC_OPERATION_FAIL;

  bool is_conflicting = false;
  SyncFileType remote_file_type_in_metadata = SYNC_FILE_TYPE_UNKNOWN;
  if (drive_metadata) {
    is_conflicting = drive_metadata->conflicted();
    remote_file_type_in_metadata =
        DriveFileSyncService::DriveMetadataResourceTypeToSyncFileType(
            drive_metadata->type());
  }

  switch (local_file_change.change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      switch (local_file_change.file_type()) {
        case SYNC_FILE_TYPE_FILE:
          return is_conflicting
              ? ResolveForAddOrUpdateFileInConflict(remote_file_change)
              : ResolveForAddOrUpdateFile(remote_file_change,
                                          remote_file_type_in_metadata);
        case SYNC_FILE_TYPE_DIRECTORY:
          return is_conflicting
              ? ResolveForAddDirectoryInConflict()
              : ResolveForAddDirectory(remote_file_change,
                                       remote_file_type_in_metadata);
        case SYNC_FILE_TYPE_UNKNOWN:
          NOTREACHED() << "Adding unknown type of local file.";
          return SYNC_OPERATION_FAIL;
      }
    case FileChange::FILE_CHANGE_DELETE:
      return is_conflicting
          ? ResolveForDeleteInConflict(remote_file_change)
          : ResolveForDelete(remote_file_change,
                             remote_file_type_in_metadata);
  }
  NOTREACHED() << "Detected unknown type of change for local file.";
  return SYNC_OPERATION_FAIL;
}

SyncOperationType LocalSyncOperationResolver::ResolveForAddOrUpdateFile(
    const FileChange* remote_file_change,
    SyncFileType remote_file_type_in_metadata) {
  if (!remote_file_change) {
    switch (remote_file_type_in_metadata) {
      case SYNC_FILE_TYPE_UNKNOWN:
        // Remote file or directory may not exist.
        return SYNC_OPERATION_ADD_FILE;
      case SYNC_FILE_TYPE_FILE:
        return SYNC_OPERATION_UPDATE_FILE;
      case SYNC_FILE_TYPE_DIRECTORY:
        return SYNC_OPERATION_RESOLVE_TO_REMOTE;
    }
    NOTREACHED() << "Detected local add-or-update to"
                 << " unknown type of remote file.";
    return SYNC_OPERATION_FAIL;
  }

  switch (remote_file_change->change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      if (remote_file_change->IsFile())
        return SYNC_OPERATION_CONFLICT;
      return SYNC_OPERATION_RESOLVE_TO_REMOTE;
    case FileChange::FILE_CHANGE_DELETE:
      return SYNC_OPERATION_RESOLVE_TO_LOCAL;
  }

  NOTREACHED() << "Local add-or-update conflicted to"
               << " unknown type of remote change.";
  return SYNC_OPERATION_FAIL;
}

SyncOperationType
LocalSyncOperationResolver::ResolveForAddOrUpdateFileInConflict(
    const FileChange* remote_file_change) {
  if (!remote_file_change)
    return SYNC_OPERATION_CONFLICT;
  switch (remote_file_change->change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      if (remote_file_change->IsFile())
        return SYNC_OPERATION_CONFLICT;
      return SYNC_OPERATION_RESOLVE_TO_REMOTE;
    case FileChange::FILE_CHANGE_DELETE:
      return SYNC_OPERATION_RESOLVE_TO_LOCAL;
  }
  NOTREACHED() << "Local add-or-update for conflicting file conflicted to"
               << " unknown type of remote change.";
  return SYNC_OPERATION_FAIL;
}

SyncOperationType LocalSyncOperationResolver::ResolveForAddDirectory(
    const FileChange* remote_file_change,
    SyncFileType remote_file_type_in_metadata) {
  if (!remote_file_change) {
    switch (remote_file_type_in_metadata) {
      case SYNC_FILE_TYPE_UNKNOWN:
        // Remote file or directory may not exist.
        return SYNC_OPERATION_ADD_DIRECTORY;
      case SYNC_FILE_TYPE_FILE:
        return SYNC_OPERATION_RESOLVE_TO_LOCAL;
      case SYNC_FILE_TYPE_DIRECTORY:
        return SYNC_OPERATION_NONE;
    }
    NOTREACHED() << "Local add directory conflicted to"
                 << " unknown type of remote file.";
    return SYNC_OPERATION_FAIL;
  }

  switch (remote_file_change->change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      if (remote_file_change->IsFile())
        return SYNC_OPERATION_RESOLVE_TO_LOCAL;
      return SYNC_OPERATION_NONE;
    case FileChange::FILE_CHANGE_DELETE:
      if (remote_file_change->IsFile())
        return SYNC_OPERATION_ADD_DIRECTORY;
      return SYNC_OPERATION_RESOLVE_TO_LOCAL;
  }

  NOTREACHED() << "Local add directory conflicted to"
               << " unknown type of remote change.";
  return SYNC_OPERATION_FAIL;
}

SyncOperationType
LocalSyncOperationResolver::ResolveForAddDirectoryInConflict() {
  return SYNC_OPERATION_RESOLVE_TO_LOCAL;
}

SyncOperationType LocalSyncOperationResolver::ResolveForDelete(
    const FileChange* remote_file_change,
    SyncFileType remote_file_type_in_metadata) {
  if (!remote_file_change)
    return SYNC_OPERATION_DELETE;

  switch (remote_file_change->change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      return SYNC_OPERATION_RESOLVE_TO_REMOTE;
    case FileChange::FILE_CHANGE_DELETE:
      return SYNC_OPERATION_DELETE_METADATA;
  }

  NOTREACHED() << "Local file deletion conflicted to"
               << " unknown type of remote change.";
  return SYNC_OPERATION_FAIL;
}

SyncOperationType LocalSyncOperationResolver::ResolveForDeleteInConflict(
    const FileChange* remote_file_change) {
  if (!remote_file_change)
    return SYNC_OPERATION_RESOLVE_TO_REMOTE;
  switch (remote_file_change->change()) {
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      return SYNC_OPERATION_RESOLVE_TO_REMOTE;
    case FileChange::FILE_CHANGE_DELETE:
      return SYNC_OPERATION_DELETE_METADATA;
  }
  NOTREACHED() << "Local file deletion for conflicting file conflicted to"
               << " unknown type of remote change.";
  return SYNC_OPERATION_FAIL;
}

}  // namespace sync_file_system
