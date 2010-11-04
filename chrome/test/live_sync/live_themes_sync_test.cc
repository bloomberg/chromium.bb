// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/live_sync/live_themes_sync_test.h"

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/ref_counted.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"

LiveThemesSyncTest::LiveThemesSyncTest(TestType test_type)
    : LiveSyncTest(test_type) {}

LiveThemesSyncTest::~LiveThemesSyncTest() {}

namespace {

scoped_refptr<Extension> MakeTheme(const ScopedTempDir& scoped_temp_dir,
                                   int index) {
  DictionaryValue source;
  source.SetString(
      extension_manifest_keys::kName,
      std::string("ThemeExtension") + base::IntToString(index));
  source.SetString(extension_manifest_keys::kVersion, "0.0.0.0");
  source.Set(extension_manifest_keys::kTheme, new DictionaryValue());
  FilePath theme_dir;
  if (!file_util::CreateTemporaryDirInDir(scoped_temp_dir.path(),
                                          FILE_PATH_LITERAL("faketheme"),
                                          &theme_dir)) {
    return NULL;
  }
  std::string error;
  scoped_refptr<Extension> extension =
      Extension::Create(theme_dir,
                        Extension::INTERNAL, source, false, &error);
  if (!error.empty()) {
    LOG(WARNING) << error;
    return NULL;
  }
  return extension;
}

}  // namespace

bool LiveThemesSyncTest::SetupClients() {
  if (!LiveSyncTest::SetupClients())
    return false;

  for (int i = 0; i < num_clients(); ++i) {
    GetProfile(i)->InitExtensions();
  }
  verifier()->InitExtensions();

  if (!theme_dir_.CreateUniqueTempDir())
    return false;

  for (int i = 0; i < num_clients(); ++i) {
    scoped_refptr<Extension> theme = MakeTheme(theme_dir_, i);
    if (!theme.get())
      return false;
    themes_.push_back(theme);
  }

  return true;
}

scoped_refptr<Extension> LiveThemesSyncTest::GetTheme(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, static_cast<int>(themes_.size()));
  return themes_[index];
}

void LiveThemesSyncTest::SetTheme(
    Profile* profile, scoped_refptr<Extension> theme) {
  CHECK(profile);
  CHECK(theme.get());
  CHECK(theme->is_theme());
  profile->GetExtensionsService()->OnExtensionInstalled(theme, true);
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
