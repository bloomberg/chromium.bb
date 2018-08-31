// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include "base/barrier_closure.h"
#include "base/base64.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/chromeos/arc/fileapi/chrome_content_provider_url_util.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/drive/file_system_core_util.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace file_manager {
namespace util {

namespace {

const char kDownloadsFolderName[] = "Downloads";
const char kGoogleDriveDisplayName[] = "Google Drive";
const char kRootRelativeToDriveMount[] = "root";
const char kTeamDrivesRelativeToDriveMount[] = "team_drives";

// Sync with the file provider in ARC++ side.
constexpr char kArcFileProviderUrl[] =
    "content://org.chromium.arc.intent_helper.fileprovider/";
// Sync with the root name defined with the file provider in ARC++ side.
constexpr base::FilePath::CharType kArcDownloadRoot[] =
    FILE_PATH_LITERAL("/download");
constexpr base::FilePath::CharType kArcExternalFilesRoot[] =
    FILE_PATH_LITERAL("/external_files");
// Sync with the removable media provider in ARC++ side.
constexpr char kArcRemovableMediaProviderUrl[] =
    "content://org.chromium.arc.removablemediaprovider/";

Profile* GetPrimaryProfile() {
  if (!user_manager::UserManager::IsInitialized())
    return nullptr;
  const auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user)
    return nullptr;
  return chromeos::ProfileHelper::Get()->GetProfileByUser(primary_user);
}

// Helper function for |ConvertToContentUrls|.
void OnSingleContentUrlResolved(const base::RepeatingClosure& barrier_closure,
                                std::vector<GURL>* out_urls,
                                size_t index,
                                const GURL& url) {
  (*out_urls)[index] = url;
  barrier_closure.Run();
}

// Helper function for |ConvertToContentUrls|.
void OnAllContentUrlsResolved(ConvertToContentUrlsCallback callback,
                              std::unique_ptr<std::vector<GURL>> urls) {
  std::move(callback).Run(*urls);
}

}  // namespace

const base::FilePath::CharType kRemovableMediaPath[] =
    FILE_PATH_LITERAL("/media/removable");

const base::FilePath::CharType kAndroidFilesPath[] =
    FILE_PATH_LITERAL("/run/arc/sdcard/write/emulated/0");

base::FilePath GetDownloadsFolderForProfile(Profile* profile) {
  // On non-ChromeOS system (test+development), the primary profile uses
  // $HOME/Downloads for ease for accessing local files for debugging.
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      user_manager::UserManager::IsInitialized()) {
    const user_manager::User* const user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(
            profile->GetOriginalProfile());
    const user_manager::User* const primary_user =
        user_manager::UserManager::Get()->GetPrimaryUser();
    if (user == primary_user)
      return DownloadPrefs::GetDefaultDownloadDirectory();
  }
  return profile->GetPath().AppendASCII(kDownloadsFolderName);
}

bool MigratePathFromOldFormat(Profile* profile,
                              const base::FilePath& old_path,
                              base::FilePath* new_path) {
  const base::FilePath old_base = DownloadPrefs::GetDefaultDownloadDirectory();
  const base::FilePath new_base = GetDownloadsFolderForProfile(profile);

  base::FilePath relative;
  if (old_path == old_base ||
      old_base.AppendRelativePath(old_path, &relative)) {
    *new_path = new_base.Append(relative);
    return old_path != *new_path;
  }

  return false;
}

std::string GetDownloadsMountPointName(Profile* profile) {
  // To distinguish profiles in multi-profile session, we append user name hash
  // to "Downloads". Note that some profiles (like login or test profiles)
  // are not associated with an user account. In that case, no suffix is added
  // because such a profile never belongs to a multi-profile session.
  const user_manager::User* const user =
      user_manager::UserManager::IsInitialized()
          ? chromeos::ProfileHelper::Get()->GetUserByProfile(
                profile->GetOriginalProfile())
          : nullptr;
  const std::string id = user ? "-" + user->username_hash() : "";
  return net::EscapeQueryParamValue(kDownloadsFolderName + id, false);
}

std::string GetCrostiniMountPointName(Profile* profile) {
  // crostini_<hash>_termina_penguin
  return base::JoinString(
      {"crostini", CryptohomeIdForProfile(profile), kCrostiniDefaultVmName,
       kCrostiniDefaultContainerName},
      "_");
}

base::FilePath GetCrostiniMountDirectory(Profile* profile) {
  return base::FilePath("/media/fuse/" + GetCrostiniMountPointName(profile));
}

std::vector<std::string> GetCrostiniMountOptions(
    const std::string& hostname,
    const std::string& host_private_key,
    const std::string& container_public_key) {
  const std::string port = "2222";
  std::vector<std::string> options;
  std::string base64_known_hosts;
  std::string base64_identity;
  base::Base64Encode(host_private_key, &base64_identity);
  base::Base64Encode(
      base::StringPrintf("[%s]:%s %s", hostname.c_str(), port.c_str(),
                         container_public_key.c_str()),
      &base64_known_hosts);
  options.push_back("UserKnownHostsBase64=" + base64_known_hosts);
  options.push_back("IdentityBase64=" + base64_identity);
  options.push_back("Port=" + port);
  return options;
}

std::string ConvertFileSystemURLToPathInsideCrostini(
    Profile* profile,
    const storage::FileSystemURL& file_system_url) {
  DCHECK(file_system_url.mount_type() == storage::kFileSystemTypeExternal);
  DCHECK(file_system_url.type() == storage::kFileSystemTypeNativeLocal);

  // Reformat virtual_path()
  // from <mount_label>/path/to/file
  // to   /<home-directory>/path/to/file
  base::FilePath folder(util::GetCrostiniMountPointName(profile));
  base::FilePath result = HomeDirectoryForProfile(profile);
  bool success =
      folder.AppendRelativePath(file_system_url.virtual_path(), &result);
  DCHECK(success);
  return result.AsUTF8Unsafe();
}

bool ConvertPathToArcUrl(const base::FilePath& path, GURL* arc_url_out) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Obtain the primary profile. This information is required because currently
  // only the file systems for the primary profile is exposed to ARC.
  Profile* primary_profile = GetPrimaryProfile();
  if (!primary_profile)
    return false;

  // Convert paths under primary profile's Downloads directory.
  base::FilePath primary_downloads =
      GetDownloadsFolderForProfile(primary_profile);
  base::FilePath result_path(kArcDownloadRoot);
  if (primary_downloads.AppendRelativePath(path, &result_path)) {
    *arc_url_out = GURL(kArcFileProviderUrl)
                       .Resolve(net::EscapePath(result_path.AsUTF8Unsafe()));
    return true;
  }

  // Convert paths under Android files root (/run/arc/sdcard/write/emulated/0).
  result_path = base::FilePath(kArcExternalFilesRoot);
  if (base::FilePath(kAndroidFilesPath)
          .AppendRelativePath(path, &result_path)) {
    *arc_url_out = GURL(kArcFileProviderUrl)
                       .Resolve(net::EscapePath(result_path.AsUTF8Unsafe()));
    return true;
  }

  // Convert paths under /media/removable.
  base::FilePath relative_path;
  if (base::FilePath(kRemovableMediaPath)
          .AppendRelativePath(path, &relative_path)) {
    *arc_url_out = GURL(kArcRemovableMediaProviderUrl)
                       .Resolve(net::EscapePath(relative_path.AsUTF8Unsafe()));
    return true;
  }

  // Convert paths under /special.
  GURL external_file_url =
      chromeos::CreateExternalFileURLFromPath(primary_profile, path);
  if (!external_file_url.is_empty()) {
    *arc_url_out = arc::EncodeToChromeContentProviderUrl(external_file_url);
    return true;
  }

  // TODO(kinaba): Add conversion logic once other file systems are supported.
  return false;
}

void ConvertToContentUrls(
    const std::vector<storage::FileSystemURL>& file_system_urls,
    ConvertToContentUrlsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (file_system_urls.empty()) {
    std::move(callback).Run(std::vector<GURL>());
    return;
  }

  Profile* profile = GetPrimaryProfile();
  auto* documents_provider_root_map =
      profile ? arc::ArcDocumentsProviderRootMap::GetForBrowserContext(profile)
              : nullptr;

  // To keep the original order, prefill |out_urls| with empty URLs and
  // specify index when updating it like (*out_urls)[index] = url.
  auto out_urls = std::make_unique<std::vector<GURL>>(file_system_urls.size());
  auto* out_urls_ptr = out_urls.get();
  auto barrier = base::BarrierClosure(
      file_system_urls.size(),
      base::BindOnce(&OnAllContentUrlsResolved, std::move(callback),
                     std::move(out_urls)));
  auto single_content_url_callback =
      base::BindRepeating(&OnSingleContentUrlResolved, barrier, out_urls_ptr);

  for (size_t index = 0; index < file_system_urls.size(); ++index) {
    const auto& file_system_url = file_system_urls[index];
    GURL arc_url;
    if (file_system_url.mount_type() == storage::kFileSystemTypeExternal &&
        ConvertPathToArcUrl(file_system_url.path(), &arc_url)) {
      single_content_url_callback.Run(index, arc_url);
      continue;
    }

    if (!documents_provider_root_map) {
      single_content_url_callback.Run(index, GURL());
      continue;
    }

    base::FilePath filepath;
    auto* documents_provider_root =
        documents_provider_root_map->ParseAndLookup(file_system_url, &filepath);
    if (!documents_provider_root) {
      single_content_url_callback.Run(index, GURL());
      continue;
    }

    documents_provider_root->ResolveToContentUrl(
        filepath, base::BindRepeating(single_content_url_callback, index));
  }
}

bool ReplacePrefix(std::string* s,
                   const std::string& prefix,
                   const std::string& replacement) {
  if (base::StartsWith(*s, prefix, base::CompareCase::SENSITIVE)) {
    base::ReplaceFirstSubstringAfterOffset(s, 0, prefix, replacement);
    return true;
  }
  return false;
}

std::string GetDownloadLocationText(Profile* profile, const std::string& path) {
  std::string result(path);
  if (ReplacePrefix(&result, "/home/chronos/user/Downloads",
                    kDownloadsFolderName)) {
  } else if (ReplacePrefix(&result,
                           "/home/chronos/" +
                               profile->GetPath().BaseName().value() +
                               "/Downloads",
                           kDownloadsFolderName)) {
  } else if (ReplacePrefix(&result,
                           drive::util::GetDriveMountPointPath(profile)
                               .Append(kRootRelativeToDriveMount)
                               .value(),
                           kGoogleDriveDisplayName)) {
  } else if (ReplacePrefix(&result,
                           drive::util::GetDriveMountPointPath(profile)
                               .Append(kTeamDrivesRelativeToDriveMount)
                               .value(),
                           l10n_util::GetStringUTF8(
                               IDS_FILE_BROWSER_DRIVE_TEAM_DRIVES_LABEL))) {
  } else if (ReplacePrefix(&result, kAndroidFilesPath,
                           l10n_util::GetStringUTF8(
                               IDS_FILE_BROWSER_ANDROID_FILES_ROOT_LABEL))) {
  } else if (ReplacePrefix(&result, GetCrostiniMountDirectory(profile).value(),
                           l10n_util::GetStringUTF8(
                               IDS_FILE_BROWSER_LINUX_FILES_ROOT_LABEL))) {
  }

  base::ReplaceChars(result, "/", " \u203a ", &result);
  return result;
}

}  // namespace util
}  // namespace file_manager
