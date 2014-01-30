// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/syncable_file_system_util.h"

#include <vector>

#include "base/command_line.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_observers.h"
#include "webkit/browser/fileapi/file_system_context.h"
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

// A command switch to enable V2 Sync FileSystem.
const char kEnableSyncFileSystemV2[] = "enable-syncfs-v2";

// A command switch to specify comma-separated app IDs to enable V2 Sync
// FileSystem.
const char kSyncFileSystemV2Whitelist[] = "syncfs-v2-whitelist";

const char kSyncableMountName[] = "syncfs";
const char kSyncableMountNameForInternalSync[] = "syncfs-internal";

const base::FilePath::CharType kSyncFileSystemDir[] =
    FILE_PATH_LITERAL("Sync FileSystem");
const base::FilePath::CharType kSyncFileSystemDirDev[] =
    FILE_PATH_LITERAL("Sync FileSystem Dev");

// Flags to enable features for testing.
bool g_is_directory_operation_enabled = false;
bool g_is_syncfs_v2_enabled = false;

}  // namespace

void RegisterSyncableFileSystem() {
  ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      kSyncableMountName,
      fileapi::kFileSystemTypeSyncable,
      fileapi::FileSystemMountOption(),
      base::FilePath());
  ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      kSyncableMountNameForInternalSync,
      fileapi::kFileSystemTypeSyncableForInternalSync,
      fileapi::FileSystemMountOption(),
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
  base::FilePath path_for_url = path;
  if (fileapi::VirtualPath::IsAbsolute(path.value()))
    path_for_url = base::FilePath(path.value().substr(1));

  return ExternalMountPoints::GetSystemInstance()->CreateExternalFileSystemURL(
      origin, kSyncableMountName, path_for_url);
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
  g_is_directory_operation_enabled = flag;
}

bool IsSyncFSDirectoryOperationEnabled() {
  return IsSyncFSDirectoryOperationEnabled(GURL());
}

bool IsSyncFSDirectoryOperationEnabled(const GURL& origin) {
  return g_is_directory_operation_enabled ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          kEnableSyncFSDirectoryOperation) ||
      IsV2EnabledForOrigin(origin);
}

bool IsV2Enabled() {
  return g_is_syncfs_v2_enabled ||
        CommandLine::ForCurrentProcess()->HasSwitch(kEnableSyncFileSystemV2);
}

bool IsV2EnabledForOrigin(const GURL& origin) {
  if (IsV2Enabled())
    return true;

  // Spark release channel.
  if (origin.host() == "kcjgcakhgelcejampmijgkjkadfcncjl")
    return true;
  // Spark dev channel.
  if (origin.host() == "pnoffddplpippgcfjdhbmhkofpnaalpg")
    return true;

  CommandLine command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(kSyncFileSystemV2Whitelist)) {
    std::string app_ids_string =
        command_line.GetSwitchValueASCII(kSyncFileSystemV2Whitelist);
    if (app_ids_string.find(origin.host()) == std::string::npos)
      return false;
    std::vector<std::string> app_ids;
    Tokenize(app_ids_string, ",", &app_ids);
    for (size_t i = 0; i < app_ids.size(); ++i) {
      if (origin.host() == app_ids[i])
        return true;
    }
  }

  return false;
}

base::FilePath GetSyncFileSystemDir(const base::FilePath& profile_base_dir) {
  if (IsV2Enabled())
    return profile_base_dir.Append(kSyncFileSystemDir);
  if (IsSyncFSDirectoryOperationEnabled())
    return profile_base_dir.Append(kSyncFileSystemDirDev);
  return profile_base_dir.Append(kSyncFileSystemDir);
}

ScopedEnableSyncFSDirectoryOperation::ScopedEnableSyncFSDirectoryOperation() {
  was_enabled_ = IsSyncFSDirectoryOperationEnabled(GURL());
  SetEnableSyncFSDirectoryOperation(true);
}

ScopedEnableSyncFSDirectoryOperation::~ScopedEnableSyncFSDirectoryOperation() {
  DCHECK(IsSyncFSDirectoryOperationEnabled(GURL()));
  SetEnableSyncFSDirectoryOperation(was_enabled_);
}

ScopedEnableSyncFSV2::ScopedEnableSyncFSV2() {
  was_enabled_ = IsV2Enabled();
  g_is_syncfs_v2_enabled = true;
}

ScopedEnableSyncFSV2::~ScopedEnableSyncFSV2() {
  DCHECK(IsV2Enabled());
  g_is_syncfs_v2_enabled = was_enabled_;
}

void RunSoon(const tracked_objects::Location& from_here,
             const base::Closure& callback) {
  base::MessageLoop::current()->PostTask(from_here, callback);
}

}  // namespace sync_file_system
