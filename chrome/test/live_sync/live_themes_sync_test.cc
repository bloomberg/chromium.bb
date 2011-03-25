// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_themes_sync_test.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/extensions/extension.h"

LiveThemesSyncTest::LiveThemesSyncTest(TestType test_type)
    : LiveExtensionsSyncTestBase(test_type) {}

LiveThemesSyncTest::~LiveThemesSyncTest() {}

void LiveThemesSyncTest::SetTheme(
    Profile* profile, scoped_refptr<Extension> theme) {
  CHECK(theme->is_theme());
  InstallExtension(profile, theme);
}

void LiveThemesSyncTest::SetNativeTheme(Profile* profile) {
  ThemeServiceFactory::GetForProfile(profile)->SetNativeTheme();
}

void LiveThemesSyncTest::UseDefaultTheme(Profile* profile) {
  ThemeServiceFactory::GetForProfile(profile)->UseDefaultTheme();
}

const Extension* LiveThemesSyncTest::GetCustomTheme(
    Profile* profile) {
  return ThemeServiceFactory::GetThemeForProfile(profile);
}

bool LiveThemesSyncTest::UsingDefaultTheme(Profile* profile) {
  return !ThemeServiceFactory::GetThemeForProfile(profile) &&
      ThemeServiceFactory::GetForProfile(profile)->UsingDefaultTheme();
}

bool LiveThemesSyncTest::UsingNativeTheme(Profile* profile) {
#if defined(TOOLKIT_USES_GTK)
  const bool kHasDistinctNativeTheme = true;
#else
  const bool kHasDistinctNativeTheme = false;
#endif

  // Return true if we're not using a custom theme and we don't make a
  // distinction between the default and the system theme, or we do
  // and we're not using the default theme.
  return !ThemeServiceFactory::GetThemeForProfile(profile) &&
      (!kHasDistinctNativeTheme ||
       !ThemeServiceFactory::GetForProfile(profile)->UsingDefaultTheme());
}

bool LiveThemesSyncTest::ExtensionIsPendingInstall(
    Profile* profile, const Extension* extension) {
  const PendingExtensionManager* pending_extension_manager =
      profile->GetExtensionService()->pending_extension_manager();
  return pending_extension_manager->IsIdPending(extension->id());
}

bool LiveThemesSyncTest::HasOrWillHaveCustomTheme(
    Profile* profile, const Extension* theme) {
  return
      (GetCustomTheme(profile) == theme) ||
      ExtensionIsPendingInstall(profile, theme);
}
