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
#include "base/i18n/icu_string_conversions.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
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
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/profiles/profile_util.h"
#include "chrome/browser/drive/drive_api_util.h"
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

std::string ReadStringFromGDocFile(const base::FilePath& file_path,
                                   const std::string& key) {
  const int64 kMaxGDocSize = 4096;
  int64 file_size = 0;
  if (!base::GetFileSize(file_path, &file_size) ||
      file_size > kMaxGDocSize) {
    LOG(WARNING) << "File too large to be a GDoc file " << file_path.value();
    return std::string();
  }

  JSONFileValueSerializer reader(file_path);
  std::string error_message;
  scoped_ptr<base::Value> root_value(reader.Deserialize(NULL, &error_message));
  if (!root_value) {
    LOG(WARNING) << "Failed to parse " << file_path.value() << " as JSON."
                 << " error = " << error_message;
    return std::string();
  }

  base::DictionaryValue* dictionary_value = NULL;
  std::string result;
  if (!root_value->GetAsDictionary(&dictionary_value) ||
      !dictionary_value->GetString(key, &result)) {
    LOG(WARNING) << "No value for the given key is stored in "
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
      (kDriveGrandRootDirName));
  return grand_root_path;
}

const base::FilePath& GetDriveMyDriveRootPath() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, drive_root_path,
                         (FILE_PATH_LITERAL("drive/root")));
  return drive_root_path;
}

base::FilePath GetDriveMountPointPathForUserIdHash(
    const std::string user_id_hash) {
  static const base::FilePath::CharType kSpecialMountPointRoot[] =
      FILE_PATH_LITERAL("/special");
  static const char kDriveMountPointNameBase[] = "drive";
  return base::FilePath(kSpecialMountPointRoot).AppendASCII(
      net::EscapeQueryParamValue(
          kDriveMountPointNameBase +
          (user_id_hash.empty() ? "" : "-" + user_id_hash), false));
}

base::FilePath GetDriveMountPointPath(Profile* profile) {
  std::string id = chromeos::ProfileHelper::GetUserIdHashFromProfile(profile);
  if (id.empty() || id == chrome::kLegacyProfileDir) {
    // ProfileHelper::GetUserIdHashFromProfile works only when multi-profile is
    // enabled. In that case, we fall back to use UserManager (it basically just
    // returns currently active users's hash in such a case.) I still try
    // ProfileHelper first because it works better in tests.
    chromeos::User* const user =
        chromeos::UserManager::IsInitialized()
            ? chromeos::ProfileHelper::Get()->GetUserByProfile(
                  profile->GetOriginalProfile())
            : NULL;
    if (user)
      id = user->username_hash();
  }
  return GetDriveMountPointPathForUserIdHash(id);
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
      url.GetContent(), net::UnescapeRule::NORMAL);
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
  return !ExtractDrivePath(path).empty();
}

base::FilePath ExtractDrivePath(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);
  if (components.size() < 3)
    return base::FilePath();
  if (components[0] != FILE_PATH_LITERAL("/"))
    return base::FilePath();
  if (components[1] != FILE_PATH_LITERAL("special"))
    return base::FilePath();
  if (!StartsWithASCII(components[2], "drive", true))
    return base::FilePath();

  base::FilePath drive_path = GetDriveGrandRootPath();
  for (size_t i = 3; i < components.size(); ++i)
    drive_path = drive_path.Append(components[i]);
  return drive_path;
}

Profile* ExtractProfileFromPath(const base::FilePath& path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const std::vector<Profile*>& profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (size_t i = 0; i < profiles.size(); ++i) {
    Profile* original_profile = profiles[i]->GetOriginalProfile();
    if (original_profile == profiles[i] &&
        !chromeos::ProfileHelper::IsSigninProfile(original_profile)) {
      const base::FilePath base = GetDriveMountPointPath(original_profile);
      if (base == path || base.IsParent(path))
        return original_profile;
    }
  }
  return NULL;
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
  static const base::FilePath::CharType kFileCacheVersionDir[] =
      FILE_PATH_LITERAL("v1");
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
  DCHECK(base::IsStringUTF8(input));

  std::string output;
  if (!base::ConvertToUtf8AndNormalize(input, base::kCodepageUTF8, &output))
    output = input;
  base::ReplaceChars(output, "/", "_", &output);
  if (!output.empty() && output.find_first_not_of('.', 0) == std::string::npos)
    output = "_";
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
  return base::WriteFile(file_path, content.data(), content.size()) ==
      static_cast<int>(content.size());
}

bool HasGDocFileExtension(const base::FilePath& file_path) {
  std::string extension = base::FilePath(file_path.Extension()).AsUTF8Unsafe();
  return IsHostedDocumentByExtension(extension);
}

GURL ReadUrlFromGDocFile(const base::FilePath& file_path) {
  return GURL(ReadStringFromGDocFile(file_path, "url"));
}

std::string ReadResourceIdFromGDocFile(const base::FilePath& file_path) {
  return ReadStringFromGDocFile(file_path, "resource_id");
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

ConnectionStatusType GetDriveConnectionStatus(Profile* profile) {
  drive::DriveServiceInterface* const drive_service =
      drive::util::GetDriveServiceByProfile(profile);

  if (!drive_service)
    return DRIVE_DISCONNECTED_NOSERVICE;
  if (net::NetworkChangeNotifier::IsOffline())
    return DRIVE_DISCONNECTED_NONETWORK;
  if (!drive_service->CanSendRequest())
    return DRIVE_DISCONNECTED_NOTREADY;

  const bool is_connection_cellular =
      net::NetworkChangeNotifier::IsConnectionCellular(
          net::NetworkChangeNotifier::GetConnectionType());
  const bool disable_sync_over_celluar =
      profile->GetPrefs()->GetBoolean(prefs::kDisableDriveOverCellular);

  if (is_connection_cellular && disable_sync_over_celluar)
    return DRIVE_CONNECTED_METERED;
  return DRIVE_CONNECTED;
}

}  // namespace util
}  // namespace drive
