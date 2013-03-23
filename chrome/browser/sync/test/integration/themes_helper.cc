// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/themes_helper.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_extension_helper.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest.h"
#include "extensions/common/id_util.h"

using sync_datatype_helper::test;

namespace {

// Make a name to pass to an extension helper.
std::string MakeName(int index) {
  return "faketheme" + base::IntToString(index);
}

ThemeService* GetThemeService(Profile* profile) {
  return ThemeServiceFactory::GetForProfile(profile);
}

}  // namespace

namespace themes_helper {

std::string GetCustomTheme(int index) {
  return extensions::id_util::GenerateId(MakeName(index));
}

std::string GetThemeID(Profile* profile) {
  return GetThemeService(profile)->GetThemeID();
}

bool UsingCustomTheme(Profile* profile) {
  return GetThemeID(profile) != ThemeService::kDefaultThemeID;
}

bool UsingDefaultTheme(Profile* profile) {
  return GetThemeService(profile)->UsingDefaultTheme();
}

bool UsingNativeTheme(Profile* profile) {
  return GetThemeService(profile)->UsingNativeTheme();
}

bool ThemeIsPendingInstall(Profile* profile, const std::string& id) {
  return SyncExtensionHelper::GetInstance()->
      IsExtensionPendingInstallForSync(profile, id);
}

bool HasOrWillHaveCustomTheme(Profile* profile, const std::string& id) {
  return (GetThemeID(profile) == id) || ThemeIsPendingInstall(profile, id);
}

void UseCustomTheme(Profile* profile, int index) {
  SyncExtensionHelper::GetInstance()->InstallExtension(
      profile, MakeName(index), extensions::Manifest::TYPE_THEME);
}

void UseDefaultTheme(Profile* profile) {
  GetThemeService(profile)->UseDefaultTheme();
}

void UseNativeTheme(Profile* profile) {
  // TODO(akalin): Fix this inconsistent naming in the theme service.
  GetThemeService(profile)->SetNativeTheme();
}

}  // namespace themes_helper
