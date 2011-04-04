// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_list.h"

#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/shell_integration.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_MACOSX)
#include "base/mac/foundation_util.h"
#include "chrome/browser/importer/safari_importer.h"
#endif

namespace {

#if defined(OS_WIN)
void DetectIEProfiles(std::vector<importer::ProfileInfo*>* profiles) {
    // IE always exists and doesn't have multiple profiles.
  importer::ProfileInfo* ie = new importer::ProfileInfo();
  ie->description = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_IE);
  ie->importer_type = importer::MS_IE;
  ie->source_path.clear();
  ie->app_path.clear();
  ie->services_supported = importer::HISTORY | importer::FAVORITES |
      importer::COOKIES | importer::PASSWORDS | importer::SEARCH_ENGINES;
  profiles->push_back(ie);
}
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
void DetectSafariProfiles(std::vector<importer::ProfileInfo*>* profiles) {
  uint16 items = importer::NONE;
  if (!SafariImporter::CanImport(base::mac::GetUserLibraryPath(), &items))
    return;

  importer::ProfileInfo* safari = new importer::ProfileInfo();
  safari->importer_type = importer::SAFARI;
  safari->description = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_SAFARI);
  safari->source_path.clear();
  safari->app_path.clear();
  safari->services_supported = items;
  profiles->push_back(safari);
}
#endif  // defined(OS_MACOSX)

void DetectFirefoxProfiles(std::vector<importer::ProfileInfo*>* profiles) {
  FilePath profile_path = GetFirefoxProfilePath();
  if (profile_path.empty())
    return;

  // Detects which version of Firefox is installed.
  importer::ImporterType firefox_type;
  FilePath app_path;
  int version = 0;
#if defined(OS_WIN)
  version = GetCurrentFirefoxMajorVersionFromRegistry();
#endif
  if (version < 2)
    GetFirefoxVersionAndPathFromProfile(profile_path, &version, &app_path);

  if (version == 2) {
    firefox_type = importer::FIREFOX2;
  } else if (version >= 3) {
    firefox_type = importer::FIREFOX3;
  } else {
    // Ignores other versions of firefox.
    return;
  }

  importer::ProfileInfo* firefox = new importer::ProfileInfo();
  firefox->description = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_FIREFOX);
  firefox->importer_type = firefox_type;
  firefox->source_path = profile_path;
#if defined(OS_WIN)
  firefox->app_path = FilePath::FromWStringHack(
      GetFirefoxInstallPathFromRegistry());
#endif
  if (firefox->app_path.empty())
    firefox->app_path = app_path;
  firefox->services_supported = importer::HISTORY | importer::FAVORITES |
      importer::PASSWORDS | importer::SEARCH_ENGINES;
  profiles->push_back(firefox);
}

void DetectGoogleToolbarProfiles(
    std::vector<importer::ProfileInfo*>* profiles) {
  if (FirstRun::IsChromeFirstRun())
    return;

  importer::ProfileInfo* google_toolbar = new importer::ProfileInfo();
  google_toolbar->importer_type = importer::GOOGLE_TOOLBAR5;
  google_toolbar->description =
      l10n_util::GetStringUTF16(IDS_IMPORT_FROM_GOOGLE_TOOLBAR);
  google_toolbar->source_path.clear();
  google_toolbar->app_path.clear();
  google_toolbar->services_supported = importer::FAVORITES;
  profiles->push_back(google_toolbar);
}

}  // namespace

ImporterList::ImporterList()
    : source_thread_id_(BrowserThread::UI),
      observer_(NULL),
      is_observed_(false),
      source_profiles_loaded_(false) {
}

ImporterList::~ImporterList() {
}

void ImporterList::DetectSourceProfiles(Observer* observer) {
  DCHECK(observer);
  observer_ = observer;
  is_observed_ = true;

  BrowserThread::GetCurrentThreadIdentifier(&source_thread_id_);

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      NewRunnableMethod(this, &ImporterList::DetectSourceProfilesWorker));
}

void ImporterList::SetObserver(Observer* observer) {
  observer_ = observer;
}

void ImporterList::DetectSourceProfilesHack() {
  DetectSourceProfilesWorker();
}

int ImporterList::GetAvailableProfileCount() const {
  DCHECK(source_profiles_loaded_);
  return static_cast<int>(source_profiles_.size());
}

string16 ImporterList::GetSourceProfileNameAt(int index) const {
  DCHECK(source_profiles_loaded_);
  DCHECK(index >=0 && index < GetAvailableProfileCount());
  return source_profiles_[index]->description;
}

const importer::ProfileInfo& ImporterList::GetSourceProfileInfoAt(
    int index) const {
  DCHECK(source_profiles_loaded_);
  DCHECK(index >=0 && index < GetAvailableProfileCount());
  return *source_profiles_[index];
}

const importer::ProfileInfo& ImporterList::GetSourceProfileInfoForImporterType(
    int importer_type) const {
  DCHECK(source_profiles_loaded_);

  int count = GetAvailableProfileCount();
  for (int i = 0; i < count; ++i) {
    if (source_profiles_[i]->importer_type == importer_type)
      return *source_profiles_[i];
  }
  NOTREACHED();
  return *(new importer::ProfileInfo());
}

bool ImporterList::source_profiles_loaded() const {
  return source_profiles_loaded_;
}

void ImporterList::DetectSourceProfilesWorker() {
  // TODO(jhawkins): Remove this condition once DetectSourceProfileHack is
  // removed.
  if (is_observed_)
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::vector<importer::ProfileInfo*> profiles;

// The first run import will automatically take settings from the first
// profile detected, which should be the user's current default.
#if defined(OS_WIN)
  if (ShellIntegration::IsFirefoxDefaultBrowser()) {
    DetectFirefoxProfiles(&profiles);
    DetectIEProfiles(&profiles);
  } else {
    DetectIEProfiles(&profiles);
    DetectFirefoxProfiles(&profiles);
  }
  // TODO(brg) : Current UI requires win_util.
  DetectGoogleToolbarProfiles(&profiles);
#elif defined(OS_MACOSX)
  if (ShellIntegration::IsFirefoxDefaultBrowser()) {
    DetectFirefoxProfiles(&profiles);
    DetectSafariProfiles(&profiles);
  } else {
    DetectSafariProfiles(&profiles);
    DetectFirefoxProfiles(&profiles);
  }
#else
  DetectFirefoxProfiles(&profiles);
#endif

  // TODO(jhawkins): Remove this condition once DetectSourceProfileHack is
  // removed.
  if (is_observed_) {
    BrowserThread::PostTask(
        source_thread_id_,
        FROM_HERE,
        NewRunnableMethod(this, &ImporterList::SourceProfilesLoaded, profiles));
  } else {
    source_profiles_->assign(profiles.begin(), profiles.end());
    source_profiles_loaded_ = true;
  }
}

void ImporterList::SourceProfilesLoaded(
    const std::vector<importer::ProfileInfo*>& profiles) {
  // |observer_| may be NULL if it removed itself before being notified.
  if (!observer_)
    return;

  BrowserThread::ID current_thread_id;
  BrowserThread::GetCurrentThreadIdentifier(&current_thread_id);
  DCHECK_EQ(current_thread_id, source_thread_id_);

  source_profiles_->assign(profiles.begin(), profiles.end());
  source_profiles_loaded_ = true;
  source_thread_id_ = BrowserThread::UI;

  observer_->SourceProfilesLoaded();
  observer_ = NULL;

  // TODO(jhawkins): Remove once DetectSourceProfileHack is removed.
  is_observed_ = false;
}
