// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_themes_sync_test.h"

#include <string>

#include "base/logging.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/common/extensions/extension.h"

LiveThemesSyncTest::LiveThemesSyncTest(TestType test_type)
    : LiveExtensionsSyncTestBase(test_type) {}

LiveThemesSyncTest::~LiveThemesSyncTest() {}

void LiveThemesSyncTest::SetTheme(
    Profile* profile, scoped_refptr<Extension> theme) {
  CHECK(theme->is_theme());
  InstallExtension(profile, theme);
}

const Extension* LiveThemesSyncTest::GetCustomTheme(
    Profile* profile) {
  return profile->GetTheme();
}

bool LiveThemesSyncTest::UsingDefaultTheme(Profile* profile) {
  return
      !profile->GetTheme() &&
      profile->GetThemeProvider()->UsingDefaultTheme();
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
  return
      !profile->GetTheme() &&
      (!kHasDistinctNativeTheme ||
       !profile->GetThemeProvider()->UsingDefaultTheme());
}

bool LiveThemesSyncTest::ExtensionIsPendingInstall(
    Profile* profile, const Extension* extension) {
  const PendingExtensionMap& pending_extensions =
      profile->GetExtensionsService()->pending_extensions();
  return pending_extensions.find(extension->id()) != pending_extensions.end();
}

bool LiveThemesSyncTest::HasOrWillHaveCustomTheme(
    Profile* profile, const Extension* theme) {
  return
      (GetCustomTheme(profile) == theme) ||
      ExtensionIsPendingInstall(profile, theme);
}
