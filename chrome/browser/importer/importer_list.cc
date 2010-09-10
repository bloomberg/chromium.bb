// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_list.h"

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/stl_util-inl.h"
#include "base/values.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/importer/firefox2_importer.h"
#include "chrome/browser/importer/firefox3_importer.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/toolbar_importer.h"
#include "chrome/browser/shell_integration.h"
#include "grit/generated_resources.h"

#if defined(OS_WIN)
#include "chrome/browser/importer/ie_importer.h"
#include "chrome/browser/password_manager/ie7_password.h"
#endif
#if defined(OS_MACOSX)
#include "base/mac_util.h"
#include "chrome/browser/importer/safari_importer.h"
#endif

ImporterList::ImporterList() {
}

ImporterList::~ImporterList() {
  STLDeleteContainerPointers(source_profiles_.begin(), source_profiles_.end());
}

void ImporterList::DetectSourceProfiles() {
#if defined(OS_WIN)
  // The order in which detect is called determines the order
  // in which the options appear in the dropdown combo-box
  if (ShellIntegration::IsFirefoxDefaultBrowser()) {
    DetectFirefoxProfiles();
    DetectIEProfiles();
  } else {
    DetectIEProfiles();
    DetectFirefoxProfiles();
  }
  // TODO(brg) : Current UI requires win_util.
  DetectGoogleToolbarProfiles();
#else
#if defined(OS_MACOSX)
  DetectSafariProfiles();
#endif
  DetectFirefoxProfiles();
#endif
}

Importer* ImporterList::CreateImporterByType(importer::ProfileType type) {
  switch (type) {
#if defined(OS_WIN)
    case importer::MS_IE:
      return new IEImporter();
#endif
    case importer::BOOKMARKS_HTML:
    case importer::FIREFOX2:
      return new Firefox2Importer();
    case importer::FIREFOX3:
      return new Firefox3Importer();
    case importer::GOOGLE_TOOLBAR5:
      return new Toolbar5Importer();
#if defined(OS_MACOSX)
    case importer::SAFARI:
      return new SafariImporter(mac_util::GetUserLibraryPath());
#endif  // OS_MACOSX
  }
  NOTREACHED();
  return NULL;
}

int ImporterList::GetAvailableProfileCount() const {
  return static_cast<int>(source_profiles_.size());
}

std::wstring ImporterList::GetSourceProfileNameAt(int index) const {
  DCHECK(index >=0 && index < GetAvailableProfileCount());
  return source_profiles_[index]->description;
}

const importer::ProfileInfo& ImporterList::GetSourceProfileInfoAt(
    int index) const {
  DCHECK(index >=0 && index < GetAvailableProfileCount());
  return *source_profiles_[index];
}

const importer::ProfileInfo& ImporterList::GetSourceProfileInfoForBrowserType(
    int browser_type) const {
  int count = GetAvailableProfileCount();
  for (int i = 0; i < count; ++i) {
    if (source_profiles_[i]->browser_type == browser_type)
      return *source_profiles_[i];
  }
  NOTREACHED();
  return *(new importer::ProfileInfo());
}

#if defined(OS_WIN)
void ImporterList::DetectIEProfiles() {
  // IE always exists and don't have multiple profiles.
  ProfileInfo* ie = new ProfileInfo();
  ie->description = l10n_util::GetString(IDS_IMPORT_FROM_IE);
  ie->browser_type = importer::MS_IE;
  ie->source_path.clear();
  ie->app_path.clear();
  ie->services_supported = importer::HISTORY | importer::FAVORITES |
      importer::COOKIES | importer::PASSWORDS | importer::SEARCH_ENGINES;
  source_profiles_.push_back(ie);
}
#endif

void ImporterList::DetectFirefoxProfiles() {
  FilePath profile_path = GetFirefoxProfilePath();
  if (profile_path.empty())
    return;

  // Detects which version of Firefox is installed.
  importer::ProfileType firefox_type;
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
  firefox->description = l10n_util::GetString(IDS_IMPORT_FROM_FIREFOX);
  firefox->browser_type = firefox_type;
  firefox->source_path = profile_path;
#if defined(OS_WIN)
  firefox->app_path = FilePath::FromWStringHack(
      GetFirefoxInstallPathFromRegistry());
#endif
  if (firefox->app_path.empty())
    firefox->app_path = app_path;
  firefox->services_supported = importer::HISTORY | importer::FAVORITES |
      importer::PASSWORDS | importer::SEARCH_ENGINES;
  source_profiles_.push_back(firefox);
}

void ImporterList::DetectGoogleToolbarProfiles() {
  if (!FirstRun::IsChromeFirstRun()) {
    importer::ProfileInfo* google_toolbar = new importer::ProfileInfo();
    google_toolbar->browser_type = importer::GOOGLE_TOOLBAR5;
    google_toolbar->description = l10n_util::GetString(
                                  IDS_IMPORT_FROM_GOOGLE_TOOLBAR);
    google_toolbar->source_path.clear();
    google_toolbar->app_path.clear();
    google_toolbar->services_supported = importer::FAVORITES;
    source_profiles_.push_back(google_toolbar);
  }
}

#if defined(OS_MACOSX)
void ImporterList::DetectSafariProfiles() {
  uint16 items = importer::NONE;
  if (SafariImporter::CanImport(mac_util::GetUserLibraryPath(), &items)) {
    importer::ProfileInfo* safari = new importer::ProfileInfo();
    safari->browser_type = importer::SAFARI;
    safari->description = l10n_util::GetString(IDS_IMPORT_FROM_SAFARI);
    safari->source_path.clear();
    safari->app_path.clear();
    safari->services_supported = items;
    source_profiles_.push_back(safari);
  }
}
#endif  // OS_MACOSX
