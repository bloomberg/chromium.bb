// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_themes_sync_test.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/extensions/extension.h"

namespace {

// Make a name to pass to an extension helper.
std::string MakeName(int index) {
  return "faketheme" + base::IntToString(index);
}

ThemeService* GetThemeService(Profile* profile) {
  return ThemeServiceFactory::GetForProfile(profile);
}

}  // namespace

LiveThemesSyncTest::LiveThemesSyncTest(TestType test_type)
    : LiveSyncTest(test_type) {}

LiveThemesSyncTest::~LiveThemesSyncTest() {}

bool LiveThemesSyncTest::SetupClients() {
  if (!LiveSyncTest::SetupClients())
    return false;

  extension_helper_.Setup(this);
  return true;
}

std::string LiveThemesSyncTest::GetCustomTheme(int index) const {
  return extension_helper_.NameToId(MakeName(index));
}

std::string LiveThemesSyncTest::GetThemeID(Profile* profile) const {
  return GetThemeService(profile)->GetThemeID();
}

bool LiveThemesSyncTest::UsingCustomTheme(Profile* profile) const {
  return GetThemeID(profile) != ThemeService::kDefaultThemeID;
}

bool LiveThemesSyncTest::UsingDefaultTheme(Profile* profile) const {
  return GetThemeService(profile)->UsingDefaultTheme();
}

bool LiveThemesSyncTest::UsingNativeTheme(Profile* profile) const {
  return GetThemeService(profile)->UsingNativeTheme();
}

bool LiveThemesSyncTest::ThemeIsPendingInstall(
    Profile* profile, const std::string& id) const {
  return extension_helper_.IsExtensionPendingInstallForSync(profile, id);
}

bool LiveThemesSyncTest::HasOrWillHaveCustomTheme(
    Profile* profile, const std::string& id) const {
  return (GetThemeID(profile) == id) || ThemeIsPendingInstall(profile, id);
}

void LiveThemesSyncTest::UseCustomTheme(Profile* profile, int index) {
  extension_helper_.InstallExtension(
      profile, MakeName(index), Extension::TYPE_THEME);
}

void LiveThemesSyncTest::UseDefaultTheme(Profile* profile) {
  GetThemeService(profile)->UseDefaultTheme();
}

void LiveThemesSyncTest::UseNativeTheme(Profile* profile) {
  // TODO(akalin): Fix this inconsistent naming in the theme service.
  GetThemeService(profile)->SetNativeTheme();
}
