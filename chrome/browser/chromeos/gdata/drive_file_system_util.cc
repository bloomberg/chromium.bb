// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_file_system_util.h"

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
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_file_system_interface.h"
#include "chrome/browser/chromeos/gdata/drive_system_service.h"
#include "chrome/browser/chromeos/gdata/file_write_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "net/base/escape.h"

using content::BrowserThread;

namespace gdata {
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

void GetHostedDocumentURLBlockingThread(const FilePath& drive_cache_path,
                                        GURL* url) {
  std::string json;
  if (!file_util::ReadFileToString(drive_cache_path, &json)) {
    NOTREACHED() << "Unable to read file " << drive_cache_path.value();
    return;
  }
  DVLOG(1) << "Hosted doc content " << json;
  scoped_ptr<base::Value> val(base::JSONReader::Read(json));
  base::DictionaryValue* dict_val;
  if (!val.get() || !val->GetAsDictionary(&dict_val)) {
    NOTREACHED() << "Parse failure for " << json;
    return;
  }
  std::string edit_url;
  if (!dict_val->GetString("url", &edit_url)) {
    NOTREACHED() << "url field doesn't exist in " << json;
    return;
  }
  *url = GURL(edit_url);
  DVLOG(1) << "edit url " << *url;
}

void OpenEditURLUIThread(Profile* profile, const GURL* edit_url) {
  Browser* browser = browser::FindLastActiveWithProfile(profile);
  if (browser) {
    browser->OpenURL(content::OpenURLParams(*edit_url, content::Referrer(),
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
  OpenEditURLUIThread(profile, &edit_url);
  DVLOG(1) << "OnFindEntryByResourceId " << edit_url;
}

// Invoked upon completion of GetEntryInfoByPath initiated by
// InsertDriveCachePathPermissions.
void OnGetEntryInfoForInsertDriveCachePathsPermissions(
    Profile* profile,
    std::vector<std::pair<FilePath, int> >* cache_paths,
    const base::Closure& callback,
    DriveFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(profile);
  DCHECK(cache_paths);
  DCHECK(!callback.is_null());

  if (entry_proto.get() && !entry_proto->has_file_specific_info())
    error = DRIVE_FILE_ERROR_NOT_FOUND;

  DriveCache* cache = GetDriveCache(profile);
  if (!cache || error != DRIVE_FILE_OK) {
    callback.Run();
    return;
  }

  DCHECK(entry_proto.get());
  const std::string& resource_id = entry_proto->resource_id();
  const std::string& file_md5 = entry_proto->file_specific_info().file_md5();

  // We check permissions for raw cache file paths only for read-only
  // operations (when fileEntry.file() is called), so read only permissions
  // should be sufficient for all cache paths. For the rest of supported
  // operations the file access check is done for drive/ paths.
  cache_paths->push_back(std::make_pair(
      cache->GetCacheFilePath(resource_id, file_md5,
          DriveCache::CACHE_TYPE_PERSISTENT,
          DriveCache::CACHED_FILE_FROM_SERVER),
      kReadOnlyFilePermissions));
  // TODO(tbarzic): When we start supporting openFile operation, we may have to
  // change permission for localy modified files to match handler's permissions.
  cache_paths->push_back(std::make_pair(
      cache->GetCacheFilePath(resource_id, file_md5,
          DriveCache::CACHE_TYPE_PERSISTENT,
          DriveCache::CACHED_FILE_LOCALLY_MODIFIED),
     kReadOnlyFilePermissions));
  cache_paths->push_back(std::make_pair(
      cache->GetCacheFilePath(resource_id, file_md5,
          DriveCache::CACHE_TYPE_PERSISTENT,
          DriveCache::CACHED_FILE_MOUNTED),
     kReadOnlyFilePermissions));
  cache_paths->push_back(std::make_pair(
      cache->GetCacheFilePath(resource_id, file_md5,
          DriveCache::CACHE_TYPE_TMP,
          DriveCache::CACHED_FILE_FROM_SERVER),
      kReadOnlyFilePermissions));

  callback.Run();
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
    GURL* edit_url = new GURL();
    content::BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
        base::Bind(&GetHostedDocumentURLBlockingThread,
                   drive_cache_path, edit_url),
        base::Bind(&OpenEditURLUIThread, profile, base::Owned(edit_url)));
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

void InsertDriveCachePathsPermissions(
    Profile* profile,
    scoped_ptr<std::vector<FilePath> > drive_paths,
    std::vector<std::pair<FilePath, int> >* cache_paths,
    const base::Closure& callback) {
  DCHECK(profile);
  DCHECK(drive_paths.get());
  DCHECK(cache_paths);
  DCHECK(!callback.is_null());

  DriveFileSystemInterface* file_system = GetDriveFileSystem(profile);
  if (!file_system || drive_paths->empty()) {
    callback.Run();
    return;
  }

  // Remove one file path entry from the back of the input vector |drive_paths|.
  FilePath drive_path = drive_paths->back();
  drive_paths->pop_back();

  // Call GetEntryInfoByPath() to get file info for |drive_path| then insert
  // all possible cache paths to the output vector |cache_paths|.
  // Note that we can only process one file path at a time. Upon completion
  // of OnGetEntryInfoForInsertDriveCachePathsPermissions(), we recursively call
  // InsertDriveCachePathsPermissions() to process the next file path from the
  // back of the input vector |drive_paths| until it is empty.
  file_system->GetEntryInfoByPath(
      drive_path,
      base::Bind(&OnGetEntryInfoForInsertDriveCachePathsPermissions,
                 profile,
                 cache_paths,
                 base::Bind(&InsertDriveCachePathsPermissions,
                             profile,
                             base::Passed(&drive_paths),
                             cache_paths,
                             callback)));
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
  if (IsUnderDriveMountPoint(path)) {
    FileWriteHelper* file_write_helper = GetFileWriteHelper(profile);
    if (!file_write_helper)
      return;
    FilePath remote_path(ExtractDrivePath(path));
    file_write_helper->PrepareWritableFileAndRun(remote_path, callback);
  } else {
    if (!callback.is_null()) {
      content::BrowserThread::GetBlockingPool()->PostTask(
          FROM_HERE, base::Bind(callback, DRIVE_FILE_OK, path));
    }
  }
}

DriveFileError GDataToDriveFileError(GDataErrorCode status) {
  switch (status) {
    case HTTP_SUCCESS:
    case HTTP_CREATED:
      return DRIVE_FILE_OK;
    case HTTP_UNAUTHORIZED:
    case HTTP_FORBIDDEN:
      return DRIVE_FILE_ERROR_ACCESS_DENIED;
    case HTTP_NOT_FOUND:
      return DRIVE_FILE_ERROR_NOT_FOUND;
    case GDATA_PARSE_ERROR:
    case GDATA_FILE_ERROR:
      return DRIVE_FILE_ERROR_ABORT;
    case GDATA_NO_CONNECTION:
      return DRIVE_FILE_ERROR_NO_CONNECTION;
    default:
      return DRIVE_FILE_ERROR_FAILED;
  }
}

}  // namespace util
}  // namespace gdata
