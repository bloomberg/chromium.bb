// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_list.h"

#include "app/l10n_util.h"
#include "base/file_util.h"
#include "base/stl_util-inl.h"
#include "base/values.h"
#include "chrome/browser/first_run.h"
#include "chrome/browser/importer/firefox2_importer.h"
#include "chrome/browser/importer/firefox3_importer.h"
#include "chrome/browser/importer/firefox_importer_utils.h"
#include "chrome/browser/importer/importer_bridge.h"
#include "chrome/browser/importer/toolbar_importer.h"
#include "chrome/browser/shell_integration.h"
#include "grit/generated_resources.h"

#if defined(OS_WIN)
#include "app/win_util.h"
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

Importer* ImporterList::CreateImporterByType(ProfileType type) {
  switch (type) {
#if defined(OS_WIN)
    case MS_IE:
      return new IEImporter();
#endif
    case BOOKMARKS_HTML:
    case FIREFOX2:
      return new Firefox2Importer();
    case FIREFOX3:
      return new Firefox3Importer();
    case GOOGLE_TOOLBAR5:
      return new Toolbar5Importer();
#if defined(OS_MACOSX)
    case SAFARI:
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

const ProfileInfo& ImporterList::GetSourceProfileInfoAt(int index) const {
  DCHECK(index >=0 && index < GetAvailableProfileCount());
  return *source_profiles_[index];
}

const ProfileInfo& ImporterList::GetSourceProfileInfoForBrowserType(
    int browser_type) const {
  int count = GetAvailableProfileCount();
  for (int i = 0; i < count; ++i) {
    if (source_profiles_[i]->browser_type == browser_type)
      return *source_profiles_[i];
  }
  NOTREACHED();
  return *(new ProfileInfo());
}

#if defined(OS_WIN)
void ImporterList::DetectIEProfiles() {
  // IE always exists and don't have multiple profiles.
  ProfileInfo* ie = new ProfileInfo();
  ie->description = l10n_util::GetString(IDS_IMPORT_FROM_IE);
  ie->browser_type = MS_IE;
  ie->source_path.clear();
  ie->app_path.clear();
  ie->services_supported = HISTORY | FAVORITES | COOKIES | PASSWORDS |
      SEARCH_ENGINES;
  source_profiles_.push_back(ie);
}
#endif

void ImporterList::DetectFirefoxProfiles() {
  DictionaryValue root;
  std::wstring ini_file = GetProfilesINI().ToWStringHack();
  ParseProfileINI(ini_file, &root);

  std::wstring source_path;
  for (int i = 0; ; ++i) {
    std::wstring current_profile = L"Profile" + IntToWString(i);
    if (!root.HasKey(current_profile)) {
      // Profiles are continuously numbered. So we exit when we can't
      // find the i-th one.
      break;
    }
    std::wstring is_relative, path, profile_path;
    if (root.GetString(current_profile + L".IsRelative", &is_relative) &&
        root.GetString(current_profile + L".Path", &path)) {
#if defined(OS_WIN)
      string16 path16 = WideToUTF16Hack(path);
      ReplaceSubstringsAfterOffset(
          &path16, 0, ASCIIToUTF16("/"), ASCIIToUTF16("\\"));
      path.assign(UTF16ToWideHack(path16));
#endif

      // IsRelative=1 means the folder path would be relative to the
      // path of profiles.ini. IsRelative=0 refers to a custom profile
      // location.
      if (is_relative == L"1") {
        profile_path = file_util::GetDirectoryFromPath(ini_file);
        file_util::AppendToPath(&profile_path, path);
      } else {
        profile_path = path;
      }

      // We only import the default profile when multiple profiles exist,
      // since the other profiles are used mostly by developers for testing.
      // Otherwise, Profile0 will be imported.
      std::wstring is_default;
      if ((root.GetString(current_profile + L".Default", &is_default) &&
           is_default == L"1") || i == 0) {
        source_path = profile_path;
        // We break out of the loop when we have found the default profile.
        if (is_default == L"1")
          break;
      }
    }
  }

  // Detects which version of Firefox is installed.
  ProfileType firefox_type;
  std::wstring app_path;
  int version = 0;
#if defined(OS_WIN)
  version = GetCurrentFirefoxMajorVersionFromRegistry();
#endif
  if (version != 2 && version != 3)
    GetFirefoxVersionAndPathFromProfile(source_path, &version, &app_path);

  if (version == 2) {
    firefox_type = FIREFOX2;
  } else if (version == 3) {
    firefox_type = FIREFOX3;
  } else {
    // Ignores other versions of firefox.
    return;
  }

  if (!source_path.empty()) {
    ProfileInfo* firefox = new ProfileInfo();
    firefox->description = l10n_util::GetString(IDS_IMPORT_FROM_FIREFOX);
    firefox->browser_type = firefox_type;
    firefox->source_path = source_path;
#if defined(OS_WIN)
    firefox->app_path = GetFirefoxInstallPathFromRegistry();
#endif
    if (firefox->app_path.empty())
      firefox->app_path = app_path;
    firefox->services_supported = HISTORY | FAVORITES | COOKIES | PASSWORDS |
        SEARCH_ENGINES;
    source_profiles_.push_back(firefox);
  }
}

void ImporterList::DetectGoogleToolbarProfiles() {
  if (!FirstRun::IsChromeFirstRun()) {
    ProfileInfo* google_toolbar = new ProfileInfo();
    google_toolbar->browser_type = GOOGLE_TOOLBAR5;
    google_toolbar->description = l10n_util::GetString(
                                  IDS_IMPORT_FROM_GOOGLE_TOOLBAR);
    google_toolbar->source_path.clear();
    google_toolbar->app_path.clear();
    google_toolbar->services_supported = FAVORITES;
    source_profiles_.push_back(google_toolbar);
  }
}

#if defined(OS_MACOSX)
void ImporterList::DetectSafariProfiles() {
  uint16 items = NONE;
  if (SafariImporter::CanImport(mac_util::GetUserLibraryPath(), &items)) {
    ProfileInfo* safari = new ProfileInfo();
    safari->browser_type = SAFARI;
    safari->description = l10n_util::GetString(IDS_IMPORT_FROM_SAFARI);
    safari->source_path.clear();
    safari->app_path.clear();
    safari->services_supported = items;
    source_profiles_.push_back(safari);
  }
}
#endif  // OS_MACOSX
