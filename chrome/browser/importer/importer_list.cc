// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_list.h"

#include "base/bind.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/importer_list_observer.h"
#include "chrome/browser/shell_integration.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>

#include "base/mac/foundation_util.h"
#include "chrome/browser/importer/safari_importer.h"
#endif

using content::BrowserThread;

namespace {

#if defined(OS_WIN)
void DetectIEProfiles(std::vector<importer::SourceProfile*>* profiles) {
    // IE always exists and doesn't have multiple profiles.
  importer::SourceProfile* ie = new importer::SourceProfile;
  ie->importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_IE);
  ie->importer_type = importer::TYPE_IE;
  ie->source_path.clear();
  ie->app_path.clear();
  ie->services_supported = importer::HISTORY | importer::FAVORITES |
      importer::COOKIES | importer::PASSWORDS | importer::SEARCH_ENGINES;
  profiles->push_back(ie);
}
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
void DetectSafariProfiles(std::vector<importer::SourceProfile*>* profiles) {
  uint16 items = importer::NONE;
  if (!SafariImporter::CanImport(base::mac::GetUserLibraryPath(), &items))
    return;

  importer::SourceProfile* safari = new importer::SourceProfile;
  safari->importer_name = l10n_util::GetStringUTF16(IDS_IMPORT_FROM_SAFARI);
  safari->importer_type = importer::TYPE_SAFARI;
  safari->source_path.clear();
  safari->app_path.clear();
  safari->services_supported = items;
  profiles->push_back(safari);
}
#endif  // defined(OS_MACOSX)

void DetectFirefoxProfiles(std::vector<importer::SourceProfile*>* profiles) {
  base::FilePath profile_path = GetFirefoxProfilePath();
  if (profile_path.empty())
    return;

  // Detects which version of Firefox is installed.
  importer::ImporterType firefox_type;
  base::FilePath app_path;
  int version = 0;
#if defined(OS_WIN)
  version = GetCurrentFirefoxMajorVersionFromRegistry();
#endif
  if (version < 2)
    GetFirefoxVersionAndPathFromProfile(profile_path, &version, &app_path);

  if (version == 2) {
    firefox_type = importer::TYPE_FIREFOX2;
  } else if (version >= 3) {
    firefox_type = importer::TYPE_FIREFOX3;
  } else {
    // Ignores other versions of firefox.
    return;
  }

  importer::SourceProfile* firefox = new importer::SourceProfile;
  firefox->importer_name = GetFirefoxImporterName(app_path);
  firefox->importer_type = firefox_type;
  firefox->source_path = profile_path;
#if defined(OS_WIN)
  firefox->app_path = GetFirefoxInstallPathFromRegistry();
#endif
  if (firefox->app_path.empty())
    firefox->app_path = app_path;
  firefox->services_supported = importer::HISTORY | importer::FAVORITES |
      importer::PASSWORDS | importer::SEARCH_ENGINES;
  profiles->push_back(firefox);
}

#if defined(OS_WIN)
void DetectGoogleToolbarProfiles(
    std::vector<importer::SourceProfile*>* profiles,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter) {
  if (first_run::IsChromeFirstRun())
    return;

  importer::SourceProfile* google_toolbar = new importer::SourceProfile;
  google_toolbar->importer_name =
      l10n_util::GetStringUTF16(IDS_IMPORT_FROM_GOOGLE_TOOLBAR);
  google_toolbar->importer_type = importer::TYPE_GOOGLE_TOOLBAR5;
  google_toolbar->source_path.clear();
  google_toolbar->app_path.clear();
  google_toolbar->services_supported = importer::FAVORITES;
  google_toolbar->request_context_getter = request_context_getter;
  profiles->push_back(google_toolbar);
}
#endif

}  // namespace

ImporterList::ImporterList(
    net::URLRequestContextGetter* request_context_getter)
    : source_thread_id_(BrowserThread::UI),
      observer_(NULL),
      is_observed_(false),
      source_profiles_loaded_(false) {
 request_context_getter_ = make_scoped_refptr(request_context_getter);
}

void ImporterList::DetectSourceProfiles(
    importer::ImporterListObserver* observer) {
  DCHECK(observer);
  observer_ = observer;
  is_observed_ = true;

  bool res = BrowserThread::GetCurrentThreadIdentifier(&source_thread_id_);
  DCHECK(res);

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ImporterList::DetectSourceProfilesWorker, this));
}

void ImporterList::SetObserver(importer::ImporterListObserver* observer) {
  observer_ = observer;
}

void ImporterList::DetectSourceProfilesHack() {
  DetectSourceProfilesWorker();
}

const importer::SourceProfile& ImporterList::GetSourceProfileAt(
    size_t index) const {
  DCHECK(source_profiles_loaded_);
  DCHECK_LT(index, count());
  return *source_profiles_[index];
}

const importer::SourceProfile& ImporterList::GetSourceProfileForImporterType(
    int importer_type) const {
  DCHECK(source_profiles_loaded_);

  for (size_t i = 0; i < count(); ++i) {
    if (source_profiles_[i]->importer_type == importer_type)
      return *source_profiles_[i];
  }
  NOTREACHED();
  return *(new importer::SourceProfile);
}

ImporterList::~ImporterList() {
}

void ImporterList::DetectSourceProfilesWorker() {
  // TODO(jhawkins): Remove this condition once DetectSourceProfilesHack is
  // removed.
  if (is_observed_)
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  std::vector<importer::SourceProfile*> profiles;

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
  DetectGoogleToolbarProfiles(&profiles, request_context_getter_);
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

  // TODO(jhawkins): Remove this condition once DetectSourceProfilesHack is
  // removed.
  if (is_observed_) {
    BrowserThread::PostTask(
        source_thread_id_,
        FROM_HERE,
        base::Bind(&ImporterList::SourceProfilesLoaded, this, profiles));
  } else {
    source_profiles_.assign(profiles.begin(), profiles.end());
    source_profiles_loaded_ = true;
  }
}

void ImporterList::SourceProfilesLoaded(
    const std::vector<importer::SourceProfile*>& profiles) {
  // |observer_| may be NULL if it removed itself before being notified.
  if (!observer_)
    return;

  BrowserThread::ID current_thread_id;
  BrowserThread::GetCurrentThreadIdentifier(&current_thread_id);
  DCHECK_EQ(current_thread_id, source_thread_id_);

  source_profiles_.assign(profiles.begin(), profiles.end());
  source_profiles_loaded_ = true;
  source_thread_id_ = BrowserThread::UI;

  observer_->OnSourceProfilesLoaded();
  observer_ = NULL;

  // TODO(jhawkins): Remove once DetectSourceProfilesHack is removed.
  is_observed_ = false;
}
