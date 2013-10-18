// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system_util.h"

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_platform_file_closer.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_interface.h"
#include "chrome/browser/chromeos/drive/job_list.h"
#include "chrome/browser/chromeos/drive/write_on_cache_file.h"
#include "chrome/browser/chromeos/profiles/profile_util.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chromeos/chromeos_constants.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "webkit/browser/fileapi/file_system_url.h"

using content::BrowserThread;

namespace drive {
namespace util {

namespace {

const char kDriveMountPointPath[] = "/special/drive";

const base::FilePath::CharType kDriveMyDriveMountPointPath[] =
    FILE_PATH_LITERAL("/special/drive/root");

const base::FilePath::CharType kDriveMyDriveRootPath[] =
    FILE_PATH_LITERAL("drive/root");

const base::FilePath::CharType kFileCacheVersionDir[] =
    FILE_PATH_LITERAL("v1");

const char kSlash[] = "/";
const char kEscapedSlash[] = "\xE2\x88\x95";

const base::FilePath& GetDriveMyDriveMountPointPath() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, drive_mydrive_mount_path,
      (kDriveMyDriveMountPointPath));
  return drive_mydrive_mount_path;
}

std::string ReadStringFromGDocFile(const base::FilePath& file_path,
                                   const std::string& key) {
  const int64 kMaxGDocSize = 4096;
  int64 file_size = 0;
  if (!file_util::GetFileSize(file_path, &file_size) ||
      file_size > kMaxGDocSize) {
    DLOG(INFO) << "File too large to be a GDoc file " << file_path.value();
    return std::string();
  }

  JSONFileValueSerializer reader(file_path);
  std::string error_message;
  scoped_ptr<base::Value> root_value(reader.Deserialize(NULL, &error_message));
  if (!root_value) {
    DLOG(INFO) << "Failed to parse " << file_path.value() << " as JSON."
               << " error = " << error_message;
    return std::string();
  }

  base::DictionaryValue* dictionary_value = NULL;
  std::string result;
  if (!root_value->GetAsDictionary(&dictionary_value) ||
      !dictionary_value->GetString(key, &result)) {
    DLOG(INFO) << "No value for the given key is stored in "
               << file_path.value() << ". key = " << key;
    return std::string();
  }

  return result;
}

// Returns DriveIntegrationService instance, if Drive is enabled.
// Otherwise, NULL.
DriveIntegrationService* GetIntegrationServiceByProfile(Profile* profile) {
  DriveIntegrationService* service =
      DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!service || !service->IsMounted())
    return NULL;
  return service;
}

}  // namespace

const base::FilePath& GetDriveGrandRootPath() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, grand_root_path,
      (util::kDriveGrandRootDirName));
  return grand_root_path;
}

const base::FilePath& GetDriveMyDriveRootPath() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, drive_root_path,
      (util::kDriveMyDriveRootPath));
  return drive_root_path;
}

const base::FilePath& GetDriveMountPointPath() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, drive_mount_path,
      (base::FilePath::FromUTF8Unsafe(kDriveMountPointPath)));
  return drive_mount_path;
}

FileSystemInterface* GetFileSystemByProfile(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveIntegrationService* integration_service =
      GetIntegrationServiceByProfile(profile);
  return integration_service ? integration_service->file_system() : NULL;
}

FileSystemInterface* GetFileSystemByProfileId(void* profile_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // |profile_id| needs to be checked with ProfileManager::IsValidProfile
  // before using it.
  Profile* profile = reinterpret_cast<Profile*>(profile_id);
  if (!g_browser_process->profile_manager()->IsValidProfile(profile))
    return NULL;
  return GetFileSystemByProfile(profile);
}

DriveAppRegistry* GetDriveAppRegistryByProfile(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveIntegrationService* integration_service =
      GetIntegrationServiceByProfile(profile);
  return integration_service ?
      integration_service->drive_app_registry() :
      NULL;
}

DriveServiceInterface* GetDriveServiceByProfile(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveIntegrationService* integration_service =
      GetIntegrationServiceByProfile(profile);
  return integration_service ? integration_service->drive_service() : NULL;
}

bool IsSpecialResourceId(const std::string& resource_id) {
  return resource_id == kDriveGrandRootSpecialResourceId ||
      resource_id == kDriveOtherDirSpecialResourceId;
}

ResourceEntry CreateMyDriveRootEntry(const std::string& root_resource_id) {
  ResourceEntry mydrive_root;
  mydrive_root.mutable_file_info()->set_is_directory(true);
  mydrive_root.set_resource_id(root_resource_id);
  mydrive_root.set_parent_local_id(util::kDriveGrandRootSpecialResourceId);
  mydrive_root.set_title(util::kDriveMyDriveRootDirName);
  return mydrive_root;
}

const std::string& GetDriveMountPointPathAsString() {
  CR_DEFINE_STATIC_LOCAL(std::string, drive_mount_path_string,
      (kDriveMountPointPath));
  return drive_mount_path_string;
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

  FileSystemInterface* file_system = GetFileSystemByProfile(profile);
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

  base::FilePath drive_path = GetDriveGrandRootPath();
  GetDriveMountPointPath().AppendRelativePath(path, &drive_path);
  return drive_path;
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
      cache_base_path.Append(chromeos::kDriveCacheDirname);
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

std::string NormalizeFileName(const std::string& input) {
  DCHECK(IsStringUTF8(input));

  std::string output;
  if (!base::ConvertToUtf8AndNormalize(input, base::kCodepageUTF8, &output))
    output = input;
  ReplaceChars(output, kSlash, std::string(kEscapedSlash), &output);
  return output;
}

void PrepareWritableFileAndRun(Profile* profile,
                               const base::FilePath& path,
                               const PrepareWritableFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  FileSystemInterface* file_system = GetFileSystemByProfile(profile);
  if (!file_system || !IsUnderDriveMountPoint(path)) {
    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE, base::Bind(callback, FILE_ERROR_FAILED, base::FilePath()));
    return;
  }

  WriteOnCacheFile(file_system,
                   ExtractDrivePath(path),
                   std::string(), // mime_type
                   callback);
}

void EnsureDirectoryExists(Profile* profile,
                           const base::FilePath& directory,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  if (IsUnderDriveMountPoint(directory)) {
    FileSystemInterface* file_system = GetFileSystemByProfile(profile);
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

void EmptyFileOperationCallback(FileError error) {
}

bool CreateGDocFile(const base::FilePath& file_path,
                    const GURL& url,
                    const std::string& resource_id) {
  std::string content = base::StringPrintf(
      "{\"url\": \"%s\", \"resource_id\": \"%s\"}",
      url.spec().c_str(), resource_id.c_str());
  return file_util::WriteFile(file_path, content.data(), content.size()) ==
      static_cast<int>(content.size());
}

bool HasGDocFileExtension(const base::FilePath& file_path) {
  return google_apis::ResourceEntry::ClassifyEntryKindByFileExtension(
      file_path) &
      google_apis::ResourceEntry::KIND_OF_HOSTED_DOCUMENT;
}

GURL ReadUrlFromGDocFile(const base::FilePath& file_path) {
  return GURL(ReadStringFromGDocFile(file_path, "url"));
}

std::string ReadResourceIdFromGDocFile(const base::FilePath& file_path) {
  return ReadStringFromGDocFile(file_path, "resource_id");
}

std::string GetMd5Digest(const base::FilePath& file_path) {
  const int kBufferSize = 512 * 1024;  // 512kB.

  base::PlatformFile file = base::CreatePlatformFile(
      file_path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL, NULL);
  if (file == base::kInvalidPlatformFileValue)
    return std::string();
  base::ScopedPlatformFileCloser file_closer(&file);

  base::MD5Context context;
  base::MD5Init(&context);

  scoped_ptr<char[]> buffer(new char[kBufferSize]);
  while (true) {
    int result = base::ReadPlatformFileCurPosNoBestEffort(
        file, buffer.get(), kBufferSize);

    if (result < 0) {
      // Found an error.
      return std::string();
    }

    if (result == 0) {
      // End of file.
      break;
    }

    base::MD5Update(&context, base::StringPiece(buffer.get(), result));
  }

  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  return MD5DigestToBase16(digest);
}

bool IsDriveEnabledForProfile(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!chromeos::IsProfileAssociatedWithGaiaAccount(profile))
    return false;

  // Disable Drive if preference is set. This can happen with commandline flag
  // --disable-drive or enterprise policy, or with user settings.
  if (profile->GetPrefs()->GetBoolean(prefs::kDisableDrive))
    return false;

  return true;
}

}  // namespace util
}  // namespace drive
