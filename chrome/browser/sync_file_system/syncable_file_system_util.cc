// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/syncable_file_system_util.h"

#include "base/command_line.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_observers.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/sandbox_file_system_backend.h"
#include "webkit/common/fileapi/file_system_util.h"

using fileapi::ExternalMountPoints;
using fileapi::FileSystemContext;
using fileapi::FileSystemURL;

namespace sync_file_system {

namespace {

// A command switch to enable syncing directory operations in Sync FileSystem
// API. (http://crbug.com/161442)
// TODO(kinuko): this command-line switch should be temporary.
const char kEnableSyncFSDirectoryOperation[] =
    "enable-syncfs-directory-operation";

const char kSyncableMountName[] = "syncfs";
const char kSyncableMountNameForInternalSync[] = "syncfs-internal";

const base::FilePath::CharType kSyncFileSystemDir[] =
    FILE_PATH_LITERAL("Sync FileSystem");
const base::FilePath::CharType kSyncFileSystemDirDev[] =
    FILE_PATH_LITERAL("Sync FileSystem Dev");

bool is_directory_operation_enabled = false;

}  // namespace

void RegisterSyncableFileSystem() {
  ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      kSyncableMountName,
      fileapi::kFileSystemTypeSyncable,
      base::FilePath());
  ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      kSyncableMountNameForInternalSync,
      fileapi::kFileSystemTypeSyncableForInternalSync,
      base::FilePath());
}

void RevokeSyncableFileSystem() {
  ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      kSyncableMountName);
  ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      kSyncableMountNameForInternalSync);
}

GURL GetSyncableFileSystemRootURI(const GURL& origin) {
  return GURL(fileapi::GetExternalFileSystemRootURIString(
      origin, kSyncableMountName));
}

FileSystemURL CreateSyncableFileSystemURL(const GURL& origin,
                                          const base::FilePath& path) {
  return ExternalMountPoints::GetSystemInstance()->CreateExternalFileSystemURL(
      origin, kSyncableMountName, path);
}

FileSystemURL CreateSyncableFileSystemURLForSync(
    fileapi::FileSystemContext* file_system_context,
    const FileSystemURL& syncable_url) {
  return ExternalMountPoints::GetSystemInstance()->CreateExternalFileSystemURL(
      syncable_url.origin(),
      kSyncableMountNameForInternalSync,
      syncable_url.path());
}

bool SerializeSyncableFileSystemURL(const FileSystemURL& url,
                                    std::string* serialized_url) {
  if (!url.is_valid() || url.type() != fileapi::kFileSystemTypeSyncable)
    return false;
  *serialized_url =
      GetSyncableFileSystemRootURI(url.origin()).spec() +
      url.path().AsUTF8Unsafe();
  return true;
}

bool DeserializeSyncableFileSystemURL(
    const std::string& serialized_url, FileSystemURL* url) {
#if !defined(FILE_PATH_USES_WIN_SEPARATORS)
  DCHECK(serialized_url.find('\\') == std::string::npos);
#endif  // FILE_PATH_USES_WIN_SEPARATORS

  FileSystemURL deserialized =
      ExternalMountPoints::GetSystemInstance()->CrackURL(GURL(serialized_url));
  if (!deserialized.is_valid() ||
      deserialized.type() != fileapi::kFileSystemTypeSyncable) {
    return false;
  }

  *url = deserialized;
  return true;
}

void SetEnableSyncFSDirectoryOperation(bool flag) {
  is_directory_operation_enabled = flag;
}

bool IsSyncFSDirectoryOperationEnabled() {
  return is_directory_operation_enabled ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          kEnableSyncFSDirectoryOperation);
}

base::FilePath GetSyncFileSystemDir(const base::FilePath& profile_base_dir) {
  return profile_base_dir.Append(
      IsSyncFSDirectoryOperationEnabled() ? kSyncFileSystemDirDev
                                          : kSyncFileSystemDir);
}

ScopedEnableSyncFSDirectoryOperation::ScopedEnableSyncFSDirectoryOperation() {
  was_enabled_ = IsSyncFSDirectoryOperationEnabled();
  SetEnableSyncFSDirectoryOperation(true);
}

ScopedEnableSyncFSDirectoryOperation::~ScopedEnableSyncFSDirectoryOperation() {
  DCHECK(IsSyncFSDirectoryOperationEnabled());
  SetEnableSyncFSDirectoryOperation(was_enabled_);
}

}  // namespace sync_file_system
