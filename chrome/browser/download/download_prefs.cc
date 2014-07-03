// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_prefs.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_extensions.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/save_page_type.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/pdf/adobe_reader_info_win.h"
#endif

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadManager;

namespace {

// Consider downloads 'dangerous' if they go to the home directory on Linux and
// to the desktop on any platform.
bool DownloadPathIsDangerous(const base::FilePath& download_path) {
#if defined(OS_LINUX)
  base::FilePath home_dir = base::GetHomeDir();
  if (download_path == home_dir) {
    return true;
  }
#endif

#if defined(OS_ANDROID)
  // Android does not have a desktop dir.
  return false;
#else
  base::FilePath desktop_dir;
  if (!PathService::Get(base::DIR_USER_DESKTOP, &desktop_dir)) {
    NOTREACHED();
    return false;
  }
  return (download_path == desktop_dir);
#endif
}

class DefaultDownloadDirectory {
 public:
  const base::FilePath& path() const { return path_; }

 private:
  friend struct base::DefaultLazyInstanceTraits<DefaultDownloadDirectory>;

  DefaultDownloadDirectory() {
    if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS, &path_)) {
      NOTREACHED();
    }
    if (DownloadPathIsDangerous(path_)) {
      // This is only useful on platforms that support
      // DIR_DEFAULT_DOWNLOADS_SAFE.
      if (!PathService::Get(chrome::DIR_DEFAULT_DOWNLOADS_SAFE, &path_)) {
        NOTREACHED();
      }
    }
  }

  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(DefaultDownloadDirectory);
};

base::LazyInstance<DefaultDownloadDirectory>
    g_default_download_directory = LAZY_INSTANCE_INITIALIZER;

}  // namespace

DownloadPrefs::DownloadPrefs(Profile* profile) : profile_(profile) {
  PrefService* prefs = profile->GetPrefs();

#if defined(OS_CHROMEOS)
  // On Chrome OS, the default download directory is different for each profile.
  // If the profile-unaware default path (from GetDefaultDownloadDirectory())
  // is set (this happens during the initial preference registration in static
  // RegisterProfilePrefs()), alter by GetDefaultDownloadDirectoryForProfile().
  // file_manager::util::MigratePathFromOldFormat will do this.
  const char* path_pref[] = {
      prefs::kSaveFileDefaultDirectory,
      prefs::kDownloadDefaultDirectory
  };
  for (size_t i = 0; i < arraysize(path_pref); ++i) {
    const base::FilePath current = prefs->GetFilePath(path_pref[i]);
    base::FilePath migrated;
    if (!current.empty() &&
        file_manager::util::MigratePathFromOldFormat(
            profile_, current, &migrated)) {
      prefs->SetFilePath(path_pref[i], migrated);
    }
  }

  // Ensure that the default download directory exists.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(base::IgnoreResult(&base::CreateDirectory),
                 GetDefaultDownloadDirectoryForProfile()));
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN)
  should_open_pdf_in_adobe_reader_ =
      prefs->GetBoolean(prefs::kOpenPdfDownloadInAdobeReader);
#endif

  // If the download path is dangerous we forcefully reset it. But if we do
  // so we set a flag to make sure we only do it once, to avoid fighting
  // the user if he really wants it on an unsafe place such as the desktop.
  if (!prefs->GetBoolean(prefs::kDownloadDirUpgraded)) {
    base::FilePath current_download_dir = prefs->GetFilePath(
        prefs::kDownloadDefaultDirectory);
    if (DownloadPathIsDangerous(current_download_dir)) {
      prefs->SetFilePath(prefs::kDownloadDefaultDirectory,
                         GetDefaultDownloadDirectoryForProfile());
    }
    prefs->SetBoolean(prefs::kDownloadDirUpgraded, true);
  }

  prompt_for_download_.Init(prefs::kPromptForDownload, prefs);
  download_path_.Init(prefs::kDownloadDefaultDirectory, prefs);
  save_file_path_.Init(prefs::kSaveFileDefaultDirectory, prefs);
  save_file_type_.Init(prefs::kSaveFileType, prefs);

  // We store any file extension that should be opened automatically at
  // download completion in this pref.
  std::string extensions_to_open =
      prefs->GetString(prefs::kDownloadExtensionsToOpen);
  std::vector<std::string> extensions;
  base::SplitString(extensions_to_open, ':', &extensions);

  for (size_t i = 0; i < extensions.size(); ++i) {
#if defined(OS_POSIX)
    base::FilePath path(extensions[i]);
#elif defined(OS_WIN)
    base::FilePath path(base::UTF8ToWide(extensions[i]));
#endif
    if (!extensions[i].empty() &&
        download_util::GetFileDangerLevel(path) == download_util::NOT_DANGEROUS)
      auto_open_.insert(path.value());
  }
}

DownloadPrefs::~DownloadPrefs() {}

// static
void DownloadPrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kPromptForDownload,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterStringPref(
      prefs::kDownloadExtensionsToOpen,
      std::string(),
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kDownloadDirUpgraded,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kSaveFileType,
      content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  const base::FilePath& default_download_path = GetDefaultDownloadDirectory();
  registry->RegisterFilePathPref(
      prefs::kDownloadDefaultDirectory,
      default_download_path,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
  registry->RegisterFilePathPref(
      prefs::kSaveFileDefaultDirectory,
      default_download_path,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#if defined(OS_WIN)
  registry->RegisterBooleanPref(
      prefs::kOpenPdfDownloadInAdobeReader,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
#endif
}

base::FilePath DownloadPrefs::GetDefaultDownloadDirectoryForProfile() const {
#if defined(OS_CHROMEOS)
  return file_manager::util::GetDownloadsFolderForProfile(profile_);
#else
  return GetDefaultDownloadDirectory();
#endif
}

// static
const base::FilePath& DownloadPrefs::GetDefaultDownloadDirectory() {
  return g_default_download_directory.Get().path();
}

// static
DownloadPrefs* DownloadPrefs::FromDownloadManager(
    DownloadManager* download_manager) {
  ChromeDownloadManagerDelegate* delegate =
      static_cast<ChromeDownloadManagerDelegate*>(
          download_manager->GetDelegate());
  return delegate->download_prefs();
}

// static
DownloadPrefs* DownloadPrefs::FromBrowserContext(
    content::BrowserContext* context) {
  return FromDownloadManager(BrowserContext::GetDownloadManager(context));
}

base::FilePath DownloadPrefs::DownloadPath() const {
#if defined(OS_CHROMEOS)
  // If the download path is under /drive, and DriveIntegrationService isn't
  // available (which it isn't for incognito mode, for instance), use the
  // default download directory (/Downloads).
  if (drive::util::IsUnderDriveMountPoint(*download_path_)) {
    drive::DriveIntegrationService* integration_service =
        drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
    if (!integration_service || !integration_service->is_enabled())
      return GetDefaultDownloadDirectoryForProfile();
  }
#endif
  return *download_path_;
}

void DownloadPrefs::SetDownloadPath(const base::FilePath& path) {
  download_path_.SetValue(path);
  SetSaveFilePath(path);
}

base::FilePath DownloadPrefs::SaveFilePath() const {
  return *save_file_path_;
}

void DownloadPrefs::SetSaveFilePath(const base::FilePath& path) {
  save_file_path_.SetValue(path);
}

void DownloadPrefs::SetSaveFileType(int type) {
  save_file_type_.SetValue(type);
}

bool DownloadPrefs::PromptForDownload() const {
  // If the DownloadDirectory policy is set, then |prompt_for_download_| should
  // always be false.
  DCHECK(!download_path_.IsManaged() || !prompt_for_download_.GetValue());
  return *prompt_for_download_;
}

bool DownloadPrefs::IsDownloadPathManaged() const {
  return download_path_.IsManaged();
}

bool DownloadPrefs::IsAutoOpenUsed() const {
#if defined(OS_WIN)
  if (ShouldOpenPdfInAdobeReader())
    return true;
#endif
  return !auto_open_.empty();
}

bool DownloadPrefs::IsAutoOpenEnabledBasedOnExtension(
    const base::FilePath& path) const {
  base::FilePath::StringType extension = path.Extension();
  if (extension.empty())
    return false;
  DCHECK(extension[0] == base::FilePath::kExtensionSeparator);
  extension.erase(0, 1);
#if defined(OS_WIN)
  if (extension == FILE_PATH_LITERAL("pdf") &&
      DownloadTargetDeterminer::IsAdobeReaderUpToDate() &&
      ShouldOpenPdfInAdobeReader())
    return true;
#endif
  return auto_open_.find(extension) != auto_open_.end();
}

bool DownloadPrefs::EnableAutoOpenBasedOnExtension(
    const base::FilePath& file_name) {
  base::FilePath::StringType extension = file_name.Extension();
  if (extension.empty())
    return false;
  DCHECK(extension[0] == base::FilePath::kExtensionSeparator);
  extension.erase(0, 1);

  auto_open_.insert(extension);
  SaveAutoOpenState();
  return true;
}

void DownloadPrefs::DisableAutoOpenBasedOnExtension(
    const base::FilePath& file_name) {
  base::FilePath::StringType extension = file_name.Extension();
  if (extension.empty())
    return;
  DCHECK(extension[0] == base::FilePath::kExtensionSeparator);
  extension.erase(0, 1);
  auto_open_.erase(extension);
  SaveAutoOpenState();
}

#if defined(OS_WIN)
void DownloadPrefs::SetShouldOpenPdfInAdobeReader(bool should_open) {
  if (should_open_pdf_in_adobe_reader_ == should_open)
    return;
  should_open_pdf_in_adobe_reader_ = should_open;
  profile_->GetPrefs()->SetBoolean(prefs::kOpenPdfDownloadInAdobeReader,
                                   should_open);
}

bool DownloadPrefs::ShouldOpenPdfInAdobeReader() const {
  return should_open_pdf_in_adobe_reader_;
}
#endif

void DownloadPrefs::ResetAutoOpen() {
#if defined(OS_WIN)
  SetShouldOpenPdfInAdobeReader(false);
#endif
  auto_open_.clear();
  SaveAutoOpenState();
}

void DownloadPrefs::SaveAutoOpenState() {
  std::string extensions;
  for (AutoOpenSet::iterator it = auto_open_.begin();
       it != auto_open_.end(); ++it) {
#if defined(OS_POSIX)
    std::string this_extension = *it;
#elif defined(OS_WIN)
    // TODO(phajdan.jr): Why we're using Sys conversion here, but not in ctor?
    std::string this_extension = base::SysWideToUTF8(*it);
#endif
    extensions += this_extension + ":";
  }
  if (!extensions.empty())
    extensions.erase(extensions.size() - 1);

  profile_->GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen, extensions);
}

bool DownloadPrefs::AutoOpenCompareFunctor::operator()(
    const base::FilePath::StringType& a,
    const base::FilePath::StringType& b) const {
  return base::FilePath::CompareLessIgnoreCase(a, b);
}
