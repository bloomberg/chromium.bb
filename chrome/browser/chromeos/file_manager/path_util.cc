// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/path_util.h"

#include "base/logging.h"
#include "base/sys_info.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root_map.h"
#include "chrome/browser/chromeos/arc/fileapi/chrome_content_provider_url_util.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "components/drive/file_system_core_util.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "url/gurl.h"

namespace file_manager {
namespace util {

namespace {

const char kDownloadsFolderName[] = "Downloads";

// Sync with the file provider in ARC++ side.
constexpr char kArcFileProviderUrl[] =
    "content://org.chromium.arc.intent_helper.fileprovider/";
// Sync with the root name defined with the file provider in ARC++ side.
constexpr base::FilePath::CharType kArcDownloadRoot[] =
    FILE_PATH_LITERAL("/download");
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

void OnContentUrlResolved(ConvertToContentUrlCallback callback,
                          const GURL& url) {
  std::move(callback).Run(url);
}

}  // namespace

const base::FilePath::CharType kRemovableMediaPath[] =
    FILE_PATH_LITERAL("/media/removable");

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
          : NULL;
  const std::string id = user ? "-" + user->username_hash() : "";
  return net::EscapeQueryParamValue(kDownloadsFolderName + id, false);
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

void ConvertToContentUrl(const storage::FileSystemURL& file_system_url,
                         ConvertToContentUrlCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  GURL arc_url;
  if (file_system_url.mount_type() == storage::kFileSystemTypeExternal &&
      ConvertPathToArcUrl(file_system_url.path(), &arc_url)) {
    std::move(callback).Run(arc_url);
    return;
  }

  Profile* primary_profile = GetPrimaryProfile();
  if (!primary_profile) {
    std::move(callback).Run(GURL());
    return;
  }

  auto* documents_provider_root_map =
      arc::ArcDocumentsProviderRootMap::GetForBrowserContext(primary_profile);
  if (!documents_provider_root_map) {
    std::move(callback).Run(GURL());
    return;
  }

  base::FilePath file_path;
  auto* documents_provider_root =
      documents_provider_root_map->ParseAndLookup(file_system_url, &file_path);
  if (!documents_provider_root) {
    std::move(callback).Run(GURL());
    return;
  }

  // TODO(niwa): Remove OnContentUrlResolved once ResolveToContentUrl is
  //             migrated from base::Callback to base::OnceCallback.
  documents_provider_root->ResolveToContentUrl(
      file_path, base::Bind(&OnContentUrlResolved, base::Passed(&callback)));
}

}  // namespace util
}  // namespace file_manager
