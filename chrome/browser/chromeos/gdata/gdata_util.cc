// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_util.h"

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
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "net/base/escape.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace gdata {
namespace util {

namespace {

const char kGDataSpecialRootPath[] = "/special";

const char kGDataMountPointPath[] = "/special/drive";

const FilePath::CharType* kGDataMountPointPathComponents[] = {
  "/", "special", "drive"
};

const FilePath::CharType* kGDataSearchPathComponents[] = {
  "drive", ".search"
};

const int kReadOnlyFilePermissions = base::PLATFORM_FILE_OPEN |
                                     base::PLATFORM_FILE_READ |
                                     base::PLATFORM_FILE_EXCLUSIVE_READ |
                                     base::PLATFORM_FILE_ASYNC;

GDataFileSystem* GetGDataFileSystem(Profile* profile) {
  GDataSystemService* system_service =
      GDataSystemServiceFactory::GetForProfile(profile);
  return system_service ? system_service->file_system() : NULL;
}

void GetHostedDocumentURLBlockingThread(const FilePath& gdata_cache_path,
                                        GURL* url) {
  std::string json;
  if (!file_util::ReadFileToString(gdata_cache_path, &json)) {
    NOTREACHED() << "Unable to read file " << gdata_cache_path.value();
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

void OpenEditURLUIThread(Profile* profile, GURL* edit_url) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (browser) {
    browser->OpenURL(content::OpenURLParams(*edit_url, content::Referrer(),
        CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));
  }
}

}  // namespace

const FilePath& GetGDataMountPointPath() {
  CR_DEFINE_STATIC_LOCAL(FilePath, gdata_mount_path,
      (FilePath::FromUTF8Unsafe(kGDataMountPointPath)));
  return gdata_mount_path;
}

const std::string& GetGDataMountPointPathAsString() {
  CR_DEFINE_STATIC_LOCAL(std::string, gdata_mount_path_string,
      (kGDataMountPointPath));
  return gdata_mount_path_string;
}

const FilePath& GetSpecialRemoteRootPath() {
  CR_DEFINE_STATIC_LOCAL(FilePath, gdata_mount_path,
      (FilePath::FromUTF8Unsafe(kGDataSpecialRootPath)));
  return gdata_mount_path;
}

GURL GetFileResourceUrl(const std::string& resource_id,
                        const std::string& file_name) {
  std::string url(base::StringPrintf(
      "%s:%s",
      chrome::kDriveScheme,
      net::EscapePath(resource_id).c_str()));
  return GURL(url);
}

void ModifyGDataFileResourceUrl(Profile* profile,
                                const FilePath& gdata_cache_path,
                                GURL* url) {
  GDataFileSystem* file_system = GetGDataFileSystem(profile);
  if (!file_system)
    return;

  // Handle hosted documents. The edit url is in the temporary file, so we
  // read it on a blocking thread.
  if (file_system->GetCacheDirectoryPath(
          GDataRootDirectory::CACHE_TYPE_TMP_DOCUMENTS).IsParent(
      gdata_cache_path)) {
    GURL* edit_url = new GURL();
    content::BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
        base::Bind(&GetHostedDocumentURLBlockingThread,
                   gdata_cache_path, edit_url),
        base::Bind(&OpenEditURLUIThread, profile, base::Owned(edit_url)));
    *url = GURL();
    return;
  }

  // Handle all other gdata files.
  if (file_system->GetCacheDirectoryPath(
          GDataRootDirectory::CACHE_TYPE_TMP).IsParent(gdata_cache_path)) {
    const std::string resource_id =
        gdata_cache_path.BaseName().RemoveExtension().AsUTF8Unsafe();
    GDataEntry* entry = NULL;
    file_system->FindEntryByResourceIdSync(
        resource_id, base::Bind(&ReadOnlyFindEntryCallback, &entry));

    std::string file_name;
    if (entry && entry->AsGDataFile())
      file_name = entry->AsGDataFile()->file_name();

    *url = gdata::util::GetFileResourceUrl(resource_id, file_name);
    DVLOG(1) << "ModifyGDataFileResourceUrl " << *url;
  }
}

bool IsUnderGDataMountPoint(const FilePath& path) {
  return GetGDataMountPointPath() == path ||
         GetGDataMountPointPath().IsParent(path);
}

GDataSearchPathType GetSearchPathStatus(const FilePath& path) {
  std::vector<std::string> components;
  path.GetComponents(&components);
  return GetSearchPathStatusForPathComponents(components);
}

GDataSearchPathType GetSearchPathStatusForPathComponents(
    const std::vector<std::string>& path_components) {
  if (path_components.size() < arraysize(kGDataSearchPathComponents))
    return GDATA_SEARCH_PATH_INVALID;

  for (size_t i = 0; i < arraysize(kGDataSearchPathComponents); i++) {
    if (path_components[i] != kGDataSearchPathComponents[i])
      return GDATA_SEARCH_PATH_INVALID;
  }

  switch (path_components.size()) {
    case 2:
      return GDATA_SEARCH_PATH_ROOT;
    case 3:
      return GDATA_SEARCH_PATH_QUERY;
    case 4:
      return GDATA_SEARCH_PATH_RESULT;
    default:
      return GDATA_SEARCH_PATH_RESULT_CHILD;
  }
}

bool ParseSearchFileName(const std::string& search_file_name,
                         std::string* resource_id,
                         std::string* original_file_name) {
  DCHECK(resource_id);
  DCHECK(original_file_name);

  *resource_id = "";
  *original_file_name = "";

  size_t dot_index = search_file_name.find('.');
  if (dot_index == std::string::npos)
    return false;

  *resource_id = search_file_name.substr(0, dot_index);
  if (search_file_name.length() - 1 > dot_index)
    *original_file_name = search_file_name.substr(dot_index + 1);
  return (!resource_id->empty() && !original_file_name->empty());
}

FilePath ExtractGDataPath(const FilePath& path) {
  if (!IsUnderGDataMountPoint(path))
    return FilePath();

  std::vector<FilePath::StringType> components;
  path.GetComponents(&components);

  // -1 to include 'drive'.
  FilePath extracted;
  for (size_t i = arraysize(kGDataMountPointPathComponents) - 1;
       i < components.size(); ++i) {
    extracted = extracted.Append(components[i]);
  }
  return extracted;
}

void InsertGDataCachePathsPermissions(
    Profile* profile,
    const FilePath& gdata_path,
    std::vector<std::pair<FilePath, int> >* cache_paths ) {
  DCHECK(cache_paths);

  GDataFileSystem* file_system = GetGDataFileSystem(profile);
  if (!file_system)
    return;

  GDataFileProperties file_properties;
  file_system->GetFileInfoByPath(gdata_path, &file_properties);

  std::string resource_id = file_properties.resource_id;
  std::string file_md5 = file_properties.file_md5;

  // We check permissions for raw cache file paths only for read-only
  // operations (when fileEntry.file() is called), so read only permissions
  // should be sufficient for all cache paths. For the rest of supported
  // operations the file access check is done for drive/ paths.
  cache_paths->push_back(std::make_pair(
      file_system->GetCacheFilePath(resource_id, file_md5,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT,
          GDataFileSystem::CACHED_FILE_FROM_SERVER),
      kReadOnlyFilePermissions));
  // TODO(tbarzic): When we start supporting openFile operation, we may have to
  // change permission for localy modified files to match handler's permissions.
  cache_paths->push_back(std::make_pair(
      file_system->GetCacheFilePath(resource_id, file_md5,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT,
          GDataFileSystem::CACHED_FILE_LOCALLY_MODIFIED),
     kReadOnlyFilePermissions));
  cache_paths->push_back(std::make_pair(
      file_system->GetCacheFilePath(resource_id, file_md5,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT,
          GDataFileSystem::CACHED_FILE_MOUNTED),
     kReadOnlyFilePermissions));
  cache_paths->push_back(std::make_pair(
      file_system->GetCacheFilePath(resource_id, file_md5,
          GDataRootDirectory::CACHE_TYPE_TMP,
          GDataFileSystem::CACHED_FILE_FROM_SERVER),
      kReadOnlyFilePermissions));

}

bool IsGDataAvailable(Profile* profile) {
  if (!chromeos::UserManager::Get()->IsUserLoggedIn() ||
      chromeos::UserManager::Get()->IsLoggedInAsGuest() ||
      chromeos::UserManager::Get()->IsLoggedInAsDemoUser())
    return false;

  // Do not allow GData for incognito windows / guest mode.
  if (profile->IsOffTheRecord())
    return false;

  // Disable gdata if preference is set.  This can happen with commandline flag
  // --disable-gdata or enterprise policy, or probably with user settings too
  // in the future.
  if (profile->GetPrefs()->GetBoolean(prefs::kDisableGData))
    return false;

  return true;
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

}  // namespace util
}  // namespace gdata
