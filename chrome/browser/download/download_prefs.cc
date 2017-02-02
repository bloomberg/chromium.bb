// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_prefs.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_target_determiner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
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
using safe_browsing::FileTypePolicies;

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

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
  should_open_pdf_in_system_reader_ =
      prefs->GetBoolean(prefs::kOpenPdfDownloadInSystemReader);
#endif

  // If the download path is dangerous we forcefully reset it. But if we do
  // so we set a flag to make sure we only do it once, to avoid fighting
  // the user if they really want it on an unsafe place such as the desktop.
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

  for (const auto& extension_string : base::SplitString(
           extensions_to_open, ":",
           base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
#if defined(OS_POSIX)
    base::FilePath::StringType extension = extension_string;
#elif defined(OS_WIN)
    base::FilePath::StringType extension = base::UTF8ToWide(extension_string);
#endif
    // If it's empty or malformed or not allowed to open automatically, then
    // skip the entry. Any such entries will be dropped from preferences the
    // next time SaveAutoOpenState() is called.
    if (extension.empty() ||
        *extension.begin() == base::FilePath::kExtensionSeparator)
      continue;
    // Construct something like ".<extension>", since
    // IsAllowedToOpenAutomatically() needs a filename.
    base::FilePath filename_with_extension = base::FilePath(
        base::FilePath::StringType(1, base::FilePath::kExtensionSeparator) +
        extension);

    // Note that the list of file types that are not allowed to open
    // automatically can change in the future. When the list is tightened, it is
    // expected that some entries in the users' auto open list will get dropped
    // permanently as a result.
    if (FileTypePolicies::GetInstance()->IsAllowedToOpenAutomatically(
            filename_with_extension))
      auto_open_.insert(extension);
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
  registry->RegisterStringPref(prefs::kDownloadExtensionsToOpen, std::string());
  registry->RegisterBooleanPref(prefs::kDownloadDirUpgraded, false);
  registry->RegisterIntegerPref(prefs::kSaveFileType,
                                content::SAVE_PAGE_TYPE_AS_COMPLETE_HTML);

  const base::FilePath& default_download_path = GetDefaultDownloadDirectory();
  registry->RegisterFilePathPref(prefs::kDownloadDefaultDirectory,
                                 default_download_path);
  registry->RegisterFilePathPref(prefs::kSaveFileDefaultDirectory,
                                 default_download_path);
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
  registry->RegisterBooleanPref(prefs::kOpenPdfDownloadInSystemReader, false);
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
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
  if (ShouldOpenPdfInSystemReader())
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
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
  if (extension == FILE_PATH_LITERAL("pdf") && ShouldOpenPdfInSystemReader())
    return true;
#endif

  return auto_open_.find(extension) != auto_open_.end();
}

bool DownloadPrefs::EnableAutoOpenBasedOnExtension(
    const base::FilePath& file_name) {
  base::FilePath::StringType extension = file_name.Extension();
  if (!FileTypePolicies::GetInstance()->IsAllowedToOpenAutomatically(
          file_name))
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

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
void DownloadPrefs::SetShouldOpenPdfInSystemReader(bool should_open) {
  if (should_open_pdf_in_system_reader_ == should_open)
    return;
  should_open_pdf_in_system_reader_ = should_open;
  profile_->GetPrefs()->SetBoolean(prefs::kOpenPdfDownloadInSystemReader,
                                   should_open);
}

bool DownloadPrefs::ShouldOpenPdfInSystemReader() const {
#if defined(OS_WIN)
  if (IsAdobeReaderDefaultPDFViewer() &&
      !DownloadTargetDeterminer::IsAdobeReaderUpToDate()) {
      return false;
  }
#endif
  return should_open_pdf_in_system_reader_;
}
#endif

void DownloadPrefs::ResetAutoOpen() {
#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
  SetShouldOpenPdfInSystemReader(false);
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
