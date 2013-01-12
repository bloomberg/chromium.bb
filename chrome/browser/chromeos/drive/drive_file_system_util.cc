// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_file_system_util.h"

#include <string>
#include <vector>
#include <utility>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "chrome/browser/chromeos/drive/drive_cache.h"
#include "chrome/browser/chromeos/drive/drive_file_system_interface.h"
#include "chrome/browser/chromeos/drive/drive_system_service.h"
#include "chrome/browser/chromeos/drive/file_write_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "net/base/network_change_notifier.h"

using content::BrowserThread;

namespace drive {
namespace util {

namespace {

const char kDriveSpecialRootPath[] = "/special";

const char kDriveMountPointPath[] = "/special/drive";

const FilePath::CharType* kDriveMountPointPathComponents[] = {
  "/", "special", "drive"
};

const char kSlash[] = "/";
const char kEscapedSlash[] = "\xE2\x88\x95";

const int kReadOnlyFilePermissions = base::PLATFORM_FILE_OPEN |
                                     base::PLATFORM_FILE_READ |
                                     base::PLATFORM_FILE_EXCLUSIVE_READ |
                                     base::PLATFORM_FILE_ASYNC;

DriveFileSystemInterface* GetDriveFileSystem(Profile* profile) {
  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(profile);
  return system_service ? system_service->file_system() : NULL;
}

DriveCache* GetDriveCache(Profile* profile) {
  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(profile);
  return system_service ? system_service->cache() : NULL;
}

FileWriteHelper* GetFileWriteHelper(Profile* profile) {
  DriveSystemService* system_service =
      DriveSystemServiceFactory::GetForProfile(profile);
  return system_service ? system_service->file_write_helper() : NULL;
}

GURL GetHostedDocumentURLBlockingThread(const FilePath& drive_cache_path) {
  std::string json;
  if (!file_util::ReadFileToString(drive_cache_path, &json)) {
    NOTREACHED() << "Unable to read file " << drive_cache_path.value();
    return GURL();
  }
  DVLOG(1) << "Hosted doc content " << json;
  scoped_ptr<base::Value> val(base::JSONReader::Read(json));
  base::DictionaryValue* dict_val;
  if (!val.get() || !val->GetAsDictionary(&dict_val)) {
    NOTREACHED() << "Parse failure for " << json;
    return GURL();
  }
  std::string edit_url;
  if (!dict_val->GetString("url", &edit_url)) {
    NOTREACHED() << "url field doesn't exist in " << json;
    return GURL();
  }
  GURL url(edit_url);
  DVLOG(1) << "edit url " << url;
  return url;
}

void OpenEditURLUIThread(Profile* profile, const GURL& edit_url) {
  Browser* browser = chrome::FindLastActiveWithProfile(profile,
      chrome::HOST_DESKTOP_TYPE_ASH);
  if (browser) {
    browser->OpenURL(content::OpenURLParams(edit_url, content::Referrer(),
        CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));
  }
}

// Invoked upon completion of GetEntryInfoByResourceId initiated by
// ModifyDriveFileResourceUrl.
void OnGetEntryInfoByResourceId(Profile* profile,
                                const std::string& resource_id,
                                DriveFileError error,
                                const FilePath& /* drive_file_path */,
                                scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (error != DRIVE_FILE_OK)
    return;

  DCHECK(entry_proto.get());
  const std::string& base_name = entry_proto->base_name();
  const GURL edit_url = GetFileResourceUrl(resource_id, base_name);
  OpenEditURLUIThread(profile, edit_url);
  DVLOG(1) << "OnFindEntryByResourceId " << edit_url;
}

}  // namespace

const FilePath& GetDriveMountPointPath() {
  CR_DEFINE_STATIC_LOCAL(FilePath, drive_mount_path,
      (FilePath::FromUTF8Unsafe(kDriveMountPointPath)));
  return drive_mount_path;
}

const std::string& GetDriveMountPointPathAsString() {
  CR_DEFINE_STATIC_LOCAL(std::string, drive_mount_path_string,
      (kDriveMountPointPath));
  return drive_mount_path_string;
}

const FilePath& GetSpecialRemoteRootPath() {
  CR_DEFINE_STATIC_LOCAL(FilePath, drive_mount_path,
      (FilePath::FromUTF8Unsafe(kDriveSpecialRootPath)));
  return drive_mount_path;
}

GURL GetFileResourceUrl(const std::string& resource_id,
                        const std::string& file_name) {
  std::string url(base::StringPrintf(
      "%s:%s",
      chrome::kDriveScheme,
      net::EscapePath(resource_id).c_str()));
  return GURL(url);
}

void ModifyDriveFileResourceUrl(Profile* profile,
                                const FilePath& drive_cache_path,
                                GURL* url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DriveFileSystemInterface* file_system = GetDriveFileSystem(profile);
  if (!file_system)
    return;
  DriveCache* cache = GetDriveCache(profile);
  if (!cache)
    return;

  if (cache->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_TMP_DOCUMENTS).
      IsParent(drive_cache_path)) {
    // Handle hosted documents. The edit url is in the temporary file, so we
    // read it on a blocking thread.
    base::PostTaskAndReplyWithResult(
        content::BrowserThread::GetBlockingPool(),
        FROM_HERE,
        base::Bind(&GetHostedDocumentURLBlockingThread, drive_cache_path),
        base::Bind(&OpenEditURLUIThread, profile));
    *url = GURL();
  } else if (cache->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_TMP).
                 IsParent(drive_cache_path) ||
             cache->GetCacheDirectoryPath(DriveCache::CACHE_TYPE_PERSISTENT).
                 IsParent(drive_cache_path)) {
    // Handle all other drive files.
    const std::string resource_id =
        drive_cache_path.BaseName().RemoveExtension().AsUTF8Unsafe();
    file_system->GetEntryInfoByResourceId(
        resource_id,
        base::Bind(&OnGetEntryInfoByResourceId,
                   profile,
                   resource_id));
    *url = GURL();
  }
}

bool IsUnderDriveMountPoint(const FilePath& path) {
  return GetDriveMountPointPath() == path ||
         GetDriveMountPointPath().IsParent(path);
}

FilePath ExtractDrivePath(const FilePath& path) {
  if (!IsUnderDriveMountPoint(path))
    return FilePath();

  std::vector<FilePath::StringType> components;
  path.GetComponents(&components);

  // -1 to include 'drive'.
  FilePath extracted;
  for (size_t i = arraysize(kDriveMountPointPathComponents) - 1;
       i < components.size(); ++i) {
    extracted = extracted.Append(components[i]);
  }
  return extracted;
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

void ParseCacheFilePath(const FilePath& path,
                        std::string* resource_id,
                        std::string* md5,
                        std::string* extra_extension) {
  DCHECK(resource_id);
  DCHECK(md5);
  DCHECK(extra_extension);

  // Extract up to two extensions from the right.
  FilePath base_name = path.BaseName();
  const int kNumExtensionsToExtract = 2;
  std::vector<FilePath::StringType> extensions;
  for (int i = 0; i < kNumExtensionsToExtract; ++i) {
    FilePath::StringType extension = base_name.Extension();
    if (!extension.empty()) {
      // FilePath::Extension returns ".", so strip it.
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
                               const FilePath& path,
                               const OpenFileCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  if (IsUnderDriveMountPoint(path)) {
    FileWriteHelper* file_write_helper = GetFileWriteHelper(profile);
    if (!file_write_helper)
      return;
    FilePath remote_path(ExtractDrivePath(path));
    file_write_helper->PrepareWritableFileAndRun(remote_path, callback);
  } else {
    content::BrowserThread::GetBlockingPool()->PostTask(
        FROM_HERE, base::Bind(callback, DRIVE_FILE_OK, path));
  }
}

void EnsureDirectoryExists(Profile* profile,
                           const FilePath& directory,
                           const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!callback.is_null());
  if (IsUnderDriveMountPoint(directory)) {
    DriveFileSystemInterface* file_system = GetDriveFileSystem(profile);
    DCHECK(file_system);
    file_system->CreateDirectory(
        ExtractDrivePath(directory),
        true /* is_exclusive */,
        true /* is_recursive */,
        callback);
  } else {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, DRIVE_FILE_OK));
  }
}

DriveFileError GDataToDriveFileError(google_apis::GDataErrorCode status) {
  switch (status) {
    case google_apis::HTTP_SUCCESS:
    case google_apis::HTTP_CREATED:
      return DRIVE_FILE_OK;
    case google_apis::HTTP_UNAUTHORIZED:
    case google_apis::HTTP_FORBIDDEN:
      return DRIVE_FILE_ERROR_ACCESS_DENIED;
    case google_apis::HTTP_NOT_FOUND:
      return DRIVE_FILE_ERROR_NOT_FOUND;
    case google_apis::GDATA_PARSE_ERROR:
    case google_apis::GDATA_FILE_ERROR:
      return DRIVE_FILE_ERROR_ABORT;
    case google_apis::GDATA_NO_CONNECTION:
      return DRIVE_FILE_ERROR_NO_CONNECTION;
    case google_apis::HTTP_SERVICE_UNAVAILABLE:
    case google_apis::HTTP_INTERNAL_SERVER_ERROR:
      return DRIVE_FILE_ERROR_THROTTLED;
    default:
      return DRIVE_FILE_ERROR_FAILED;
  }
}

void ConvertProtoToPlatformFileInfo(const PlatformFileInfoProto& proto,
                                    base::PlatformFileInfo* file_info) {
  file_info->size = proto.size();
  file_info->is_directory = proto.is_directory();
  file_info->is_symbolic_link = proto.is_symbolic_link();
  file_info->last_modified = base::Time::FromInternalValue(
      proto.last_modified());
  file_info->last_accessed = base::Time::FromInternalValue(
      proto.last_accessed());
  file_info->creation_time = base::Time::FromInternalValue(
      proto.creation_time());
}

void ConvertPlatformFileInfoToProto(const base::PlatformFileInfo& file_info,
                                    PlatformFileInfoProto* proto) {
  proto->set_size(file_info.size);
  proto->set_is_directory(file_info.is_directory);
  proto->set_is_symbolic_link(file_info.is_symbolic_link);
  proto->set_last_modified(file_info.last_modified.ToInternalValue());
  proto->set_last_accessed(file_info.last_accessed.ToInternalValue());
  proto->set_creation_time(file_info.creation_time.ToInternalValue());
}

}  // namespace util
}  // namespace drive
