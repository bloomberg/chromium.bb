// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system_util.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/file_write_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "webkit/fileapi/file_system_url.h"

using content::BrowserThread;

namespace drive {
namespace util {

namespace {

const char kDriveSpecialRootPath[] = "/special";

const char kDriveMountPointPath[] = "/special/drive";

const char kDriveMyDriveMountPointPath[] = "/special/drive/root";

const base::FilePath::CharType* kDriveMountPointPathComponents[] = {
  "/", "special", "drive"
};

const base::FilePath::CharType kFileCacheVersionDir[] =
    FILE_PATH_LITERAL("v1");

const char kSlash[] = "/";
const char kEscapedSlash[] = "\xE2\x88\x95";

const int kReadOnlyFilePermissions = base::PLATFORM_FILE_OPEN |
                                     base::PLATFORM_FILE_READ |
                                     base::PLATFORM_FILE_EXCLUSIVE_READ |
                                     base::PLATFORM_FILE_ASYNC;

FileSystemInterface* GetFileSystem(Profile* profile) {
  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(profile);
  return system_service ? system_service->file_system() : NULL;
}

FileWriteHelper* GetFileWriteHelper(Profile* profile) {
  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(profile);
  return system_service ? system_service->file_write_helper() : NULL;
}

}  // namespace


const base::FilePath& GetDriveGrandRootPath() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, grand_root_path,
      (base::FilePath::FromUTF8Unsafe(util::kDriveGrandRootDirName)));
  return grand_root_path;
}

const base::FilePath& GetDriveMyDriveRootPath() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, drive_root_path,
      (base::FilePath::FromUTF8Unsafe(util::kDriveMyDriveRootPath)));
  return drive_root_path;
}

const base::FilePath& GetDriveOtherDirPath() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, other_root_path,
      (base::FilePath::FromUTF8Unsafe(util::kDriveOtherDirPath)));
  return other_root_path;
}

const base::FilePath& GetDriveMountPointPath() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, drive_mount_path,
      (base::FilePath::FromUTF8Unsafe(kDriveMountPointPath)));
  return drive_mount_path;
}

bool IsSpecialResourceId(const std::string& resource_id) {
  return resource_id == kDriveGrandRootSpecialResourceId ||
      resource_id == kDriveOtherDirSpecialResourceId;
}

ResourceEntry CreateMyDriveRootEntry(const std::string& root_resource_id) {
  ResourceEntry mydrive_root;
  mydrive_root.mutable_file_info()->set_is_directory(true);
  mydrive_root.set_resource_id(root_resource_id);
  mydrive_root.set_parent_resource_id(util::kDriveGrandRootSpecialResourceId);
  mydrive_root.set_title(util::kDriveMyDriveRootDirName);
  return mydrive_root;
}

ResourceEntry CreateOtherDirEntry() {
  ResourceEntry other_dir;
  other_dir.mutable_file_info()->set_is_directory(true);
  other_dir.set_resource_id(util::kDriveOtherDirSpecialResourceId);
  other_dir.set_parent_resource_id(util::kDriveGrandRootSpecialResourceId);
  other_dir.set_title(util::kDriveOtherDirName);
  return other_dir;
}

const std::string& GetDriveMountPointPathAsString() {
  CR_DEFINE_STATIC_LOCAL(std::string, drive_mount_path_string,
      (kDriveMountPointPath));
  return drive_mount_path_string;
}

const base::FilePath& GetSpecialRemoteRootPath() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, drive_mount_path,
      (base::FilePath::FromUTF8Unsafe(kDriveSpecialRootPath)));
  return drive_mount_path;
}

const base::FilePath& GetDriveMyDriveMountPointPath() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, drive_mydrive_mount_path,
      (base::FilePath::FromUTF8Unsafe(kDriveMyDriveMountPointPath)));
  return drive_mydrive_mount_path;
}

GURL FilePathToDriveURL(const base::FilePath& path) {
  std::string url(base::StringPrintf("%s:%s",
                                     chrome::kDriveScheme,
                                     path.AsUTF8Unsafe().c_str()));
  return GURL(url);
}

base::FilePath DriveURLToFilePath(const GURL& url) {
  if (!url.is_valid() || url.scheme() != chrome::kDriveScheme)
    return base::FilePath();
  std::string path_string = net::UnescapeURLComponent(
      url.path(), net::UnescapeRule::NORMAL);
  return base::FilePath::FromUTF8Unsafe(path_string);
}

void MaybeSetDriveURL(Profile* profile, const base::FilePath& path, GURL* url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!IsUnderDriveMountPoint(path))
    return;

  FileSystemInterface* file_system = GetFileSystem(profile);
  if (!file_system)
    return;

  *url = FilePathToDriveURL(util::ExtractDrivePath(path));
}

bool IsUnderDriveMountPoint(const base::FilePath& path) {
  return GetDriveMountPointPath() == path ||
         GetDriveMountPointPath().IsParent(path);
}

bool NeedsNamespaceMigration(const base::FilePath& path) {
  // Before migration, "My Drive" which was represented as "drive.
  // The user might use some path pointing a directory in "My Drive".
  // e.g. "drive/downloads_dir"
  // We changed the path for the "My Drive" to "drive/root", hence the user pref
  // pointing to the old path needs update to the new path.
  // e.g. "drive/root/downloads_dir"
  // If |path| already points to some directory in "drive/root", there's no need
  // to update it.
  return IsUnderDriveMountPoint(path) &&
         !(GetDriveMyDriveMountPointPath() == path ||
           GetDriveMyDriveMountPointPath().IsParent(path));
}

base::FilePath ConvertToMyDriveNamespace(const base::FilePath& path) {
  DCHECK(NeedsNamespaceMigration(path));

  // Need to migrate "/special/drive(.*)" to "/special/drive/root(.*)".
  // Append the relative path from "/special/drive".
  base::FilePath new_path(GetDriveMyDriveMountPointPath());
  GetDriveMountPointPath().AppendRelativePath(path, &new_path);
  DVLOG(1) << "Migrate download.default_directory setting from "
      << path.AsUTF8Unsafe() << " to " << new_path.AsUTF8Unsafe();
  DCHECK(!NeedsNamespaceMigration(new_path));
  return new_path;
}

base::FilePath ExtractDrivePath(const base::FilePath& path) {
  if (!IsUnderDriveMountPoint(path))
    return base::FilePath();

  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);

  // -1 to include 'drive'.
  base::FilePath extracted;
  for (size_t i = arraysize(kDriveMountPointPathComponents) - 1;
       i < components.size(); ++i) {
    extracted = extracted.Append(components[i]);
  }
  return extracted;
}

base::FilePath ExtractDrivePathFromFileSystemUrl(
    const fileapi::FileSystemURL& url) {
  if (!url.is_valid() || url.type() != fileapi::kFileSystemTypeDrive)
    return base::FilePath();
  return ExtractDrivePath(url.path());
}

base::FilePath GetCacheRootPath(Profile* profile) {
  base::FilePath cache_base_path;
  chrome::GetUserCacheDirectory(profile->GetPath(), &cache_base_path);
  base::FilePath cache_root_path =
      cache_base_path.Append(chrome::kDriveCacheDirname);
  return cache_root_path.Append(kFileCacheVersionDir);
}

std::string EscapeCacheFileName(const std::string& filename) {
  // This is based on net/base/escape.cc: net::(anonymous namespace)::Escape
  std::string escaped;
  for (size_t i = 0; i < filename.size(); ++i) {
    char c = filename[i];
    if (c == '%' || c == '.' || c == '/') {
      base::StringAppendF(&escaped, "%%%02X", c);
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

std::string UnescapeCacheFileName(const std::string& filename) {
  std::string unescaped;
  for (size_t i = 0; i < filename.size(); ++i) {
    char c = filename[i];
    if (c == '%' && i + 2 < filename.length()) {
      c = (HexDigitToInt(filename[i + 1]) << 4) +
           HexDigitToInt(filename[i + 2]);
      i += 2;
    }
    unescaped.push_back(c);
  }
  return unescaped;
}

std::string EscapeUtf8FileName(const std::string& input) {
  std::string output;
  if (ReplaceChars(input, kSlash, std::string(kEscapedSlash), &output))
    return output;

  return input;
}

std::string ExtractResourceIdFromUrl(const GURL& url) {
  return net::UnescapeURLComponent(url.ExtractFileName(),
                                   net::UnescapeRule::URL_SPECIAL_CHARS);
}

void ParseCacheFilePath(const base::FilePath& path,
                        std::string* resource_id,
                        std::string* md5,
                        std::string* extra_extension) {
  DCHECK(resource_id);
  DCHECK(md5);
  DCHECK(extra_extension);

  // Extract up to two extensions from the right.
  base::FilePath base_name = path.BaseName();
  const int kNumExtensionsToExtract = 2;
  std::vector<base::FilePath::StringType> extensions;
  for (int i = 0; i < kNumExtensionsToExtract; ++i) {
    base::FilePath::StringType extension = base_name.Extension();
    if (!extension.empty()) {
      // base::FilePath::Extension returns ".", so strip it.
      extension = UnescapeCacheFileName(extension.substr(1));
      base_name = base_name.RemoveExtension();
      extensions.push_back(extension);
    } else {
      break;
    }
  }

  // The base_name here is already stripped of extensions in the loop above.
  *resource_id = UnescapeCacheFileName(base_name.value());

  // Assign the extracted extensions to md5 and extra_extension.
  int extension_count = extensions.size();
  *md5 = (extension_count > 0) ? extensions[extension_count - 1] :
                                 std::string();
  *extra_extension = (extension_count > 1) ? extensions[extension_count - 2] :
                                             std::string();
}

void PrepareWritableFileAndRun(Profile* profile,
                               const base::FilePath& path,
                               const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  if (IsUnderDriveMountPoint(path)) {
    FileWriteHelper* file_write_helper = GetFileWriteHelper(profile);
    if (!file_write_helper)
      return;
    base::FilePath remote_path(ExtractDrivePath(path));
    file_write_helper->PrepareWritableFileAndRun(remote_path, callback);
  } else {
    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE, base::Bind(callback, FILE_ERROR_OK, path));
  }
}

void EnsureDirectoryExists(Profile* profile,
                           const base::FilePath& directory,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback.is_null());
  if (IsUnderDriveMountPoint(directory)) {
    FileSystemInterface* file_system = GetFileSystem(profile);
    DCHECK(file_system);
    file_system->CreateDirectory(
        ExtractDrivePath(directory),
        true /* is_exclusive */,
        true /* is_recursive */,
        callback);
  } else {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, FILE_ERROR_OK));
  }
}

FileError GDataToFileError(google_apis::GDataErrorCode status) {
  switch (status) {
    case google_apis::HTTP_SUCCESS:
    case google_apis::HTTP_CREATED:
    case google_apis::HTTP_NO_CONTENT:
      return FILE_ERROR_OK;
    case google_apis::HTTP_UNAUTHORIZED:
    case google_apis::HTTP_FORBIDDEN:
      return FILE_ERROR_ACCESS_DENIED;
    case google_apis::HTTP_NOT_FOUND:
      return FILE_ERROR_NOT_FOUND;
    case google_apis::GDATA_PARSE_ERROR:
    case google_apis::GDATA_FILE_ERROR:
      return FILE_ERROR_ABORT;
    case google_apis::GDATA_NO_CONNECTION:
      return FILE_ERROR_NO_CONNECTION;
    case google_apis::HTTP_SERVICE_UNAVAILABLE:
    case google_apis::HTTP_INTERNAL_SERVER_ERROR:
      return FILE_ERROR_THROTTLED;
    default:
      return FILE_ERROR_FAILED;
  }
}

void ConvertResourceEntryToPlatformFileInfo(
    const PlatformFileInfoProto& entry,
    base::PlatformFileInfo* file_info) {
  file_info->size = entry.size();
  file_info->is_directory = entry.is_directory();
  file_info->is_symbolic_link = entry.is_symbolic_link();
  file_info->last_modified = base::Time::FromInternalValue(
      entry.last_modified());
  file_info->last_accessed = base::Time::FromInternalValue(
      entry.last_accessed());
  file_info->creation_time = base::Time::FromInternalValue(
      entry.creation_time());
}

void ConvertPlatformFileInfoToResourceEntry(
    const base::PlatformFileInfo& file_info,
    PlatformFileInfoProto* entry) {
  entry->set_size(file_info.size);
  entry->set_is_directory(file_info.is_directory);
  entry->set_is_symbolic_link(file_info.is_symbolic_link);
  entry->set_last_modified(file_info.last_modified.ToInternalValue());
  entry->set_last_accessed(file_info.last_accessed.ToInternalValue());
  entry->set_creation_time(file_info.creation_time.ToInternalValue());
}

void EmptyFileOperationCallback(FileError error) {
}

}  // namespace util
}  // namespace drive
