// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/theme_util.h"

#include <string>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_updater.h"
#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#endif
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/protocol/theme_specifics.pb.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "googleurl/src/gurl.h"

namespace browser_sync {

const char kCurrentThemeClientTag[] = "current_theme";

namespace {

bool IsSystemThemeDistinctFromDefaultTheme() {
#if defined(TOOLKIT_USES_GTK)
  return true;
#else
  return false;
#endif
}

bool UseSystemTheme(Profile* profile) {
#if defined(TOOLKIT_USES_GTK)
  return GtkThemeService::GetFrom(profile)->UseGtkTheme();
#else
  return false;
#endif
}

}  // namespace

bool AreThemeSpecificsEqual(const sync_pb::ThemeSpecifics& a,
                            const sync_pb::ThemeSpecifics& b) {
  return AreThemeSpecificsEqualHelper(
      a, b, IsSystemThemeDistinctFromDefaultTheme());
}

bool AreThemeSpecificsEqualHelper(
    const sync_pb::ThemeSpecifics& a,
    const sync_pb::ThemeSpecifics& b,
    bool is_system_theme_distinct_from_default_theme) {
  if (a.use_custom_theme() != b.use_custom_theme()) {
    return false;
  }

  if (a.use_custom_theme()) {
    // We're using a custom theme, so simply compare IDs since those
    // are guaranteed unique.
    return a.custom_theme_id() == b.custom_theme_id();
  } else if (is_system_theme_distinct_from_default_theme) {
    // We're not using a custom theme, but we care about system
    // vs. default.
    return a.use_system_theme_by_default() == b.use_system_theme_by_default();
  } else {
    // We're not using a custom theme, and we don't care about system
    // vs. default.
    return true;
  }
}

namespace {

bool IsTheme(const Extension& extension) {
  return extension.is_theme();
}

}  // namespace

void SetCurrentThemeFromThemeSpecifics(
    const sync_pb::ThemeSpecifics& theme_specifics,
    Profile* profile) {
  DCHECK(profile);
  if (theme_specifics.use_custom_theme()) {
    // TODO(akalin): Figure out what to do about third-party themes
    // (i.e., those not on either Google gallery).
    std::string id(theme_specifics.custom_theme_id());
    GURL update_url(theme_specifics.custom_theme_update_url());
    VLOG(1) << "Applying theme " << id << " with update_url " << update_url;
    ExtensionServiceInterface* extensions_service =
        profile->GetExtensionService();
    CHECK(extensions_service);
    const Extension* extension = extensions_service->GetExtensionById(id, true);
    if (extension) {
      if (!extension->is_theme()) {
        VLOG(1) << "Extension " << id << " is not a theme; aborting";
        return;
      }
      if (!extensions_service->IsExtensionEnabled(id)) {
        VLOG(1) << "Theme " << id << " is not enabled; aborting";
        return;
      }
      // Get previous theme info before we set the new theme.
      std::string previous_theme_id;
      {
        const Extension* current_theme =
            ThemeServiceFactory::GetThemeForProfile(profile);
        if (current_theme) {
          DCHECK(current_theme->is_theme());
          previous_theme_id = current_theme->id();
        }
      }
      bool previous_use_system_theme = UseSystemTheme(profile);
      // An enabled theme extension with the given id was found, so
      // just set the current theme to it.
      ThemeServiceFactory::GetForProfile(profile)->SetTheme(extension);
      // Pretend the theme was just installed.
      ExtensionInstallUI::ShowThemeInfoBar(
          previous_theme_id, previous_use_system_theme,
          extension, profile);
    } else {
      // No extension with this id exists -- we must install it; we do
      // so by adding it as a pending extension and then triggering an
      // auto-update cycle.
      // Themes don't need to install silently as they just pop up an
      // informational dialog after installation instead of a
      // confirmation dialog.
      const bool kInstallSilently = false;
      const bool kEnableOnInstall = true;
      const bool kEnableIncognitoOnInstall = false;
      extensions_service->pending_extension_manager()->AddFromSync(
          id, update_url, &IsTheme,
          kInstallSilently, kEnableOnInstall, kEnableIncognitoOnInstall);
      extensions_service->CheckForUpdatesSoon();
    }
  } else if (theme_specifics.use_system_theme_by_default()) {
    ThemeServiceFactory::GetForProfile(profile)->SetNativeTheme();
  } else {
    ThemeServiceFactory::GetForProfile(profile)->UseDefaultTheme();
  }
}

bool UpdateThemeSpecificsOrSetCurrentThemeIfNecessary(
    Profile* profile, sync_pb::ThemeSpecifics* theme_specifics) {
  if (!theme_specifics->use_custom_theme() &&
      (ThemeServiceFactory::GetThemeForProfile(profile) ||
       (UseSystemTheme(profile) &&
        IsSystemThemeDistinctFromDefaultTheme()))) {
    GetThemeSpecificsFromCurrentTheme(profile, theme_specifics);
    return true;
  } else {
    SetCurrentThemeFromThemeSpecificsIfNecessary(*theme_specifics, profile);
    return false;
  }
}

void GetThemeSpecificsFromCurrentTheme(
    Profile* profile,
    sync_pb::ThemeSpecifics* theme_specifics) {
  DCHECK(profile);
  const Extension* current_theme =
      ThemeServiceFactory::GetThemeForProfile(profile);
  if (current_theme) {
    DCHECK(current_theme->is_theme());
  }
  GetThemeSpecificsFromCurrentThemeHelper(
      current_theme,
      IsSystemThemeDistinctFromDefaultTheme(),
      UseSystemTheme(profile),
      theme_specifics);
}

void GetThemeSpecificsFromCurrentThemeHelper(
    const Extension* current_theme,
    bool is_system_theme_distinct_from_default_theme,
    bool use_system_theme_by_default,
    sync_pb::ThemeSpecifics* theme_specifics) {
  bool use_custom_theme = (current_theme != NULL);
  theme_specifics->set_use_custom_theme(use_custom_theme);
  if (is_system_theme_distinct_from_default_theme) {
    theme_specifics->set_use_system_theme_by_default(
        use_system_theme_by_default);
  } else {
    DCHECK(!use_system_theme_by_default);
  }
  if (use_custom_theme) {
    DCHECK(current_theme);
    DCHECK(current_theme->is_theme());
    theme_specifics->set_custom_theme_name(current_theme->name());
    theme_specifics->set_custom_theme_id(current_theme->id());
    theme_specifics->set_custom_theme_update_url(
        current_theme->update_url().spec());
  } else {
    DCHECK(!current_theme);
    theme_specifics->clear_custom_theme_name();
    theme_specifics->clear_custom_theme_id();
    theme_specifics->clear_custom_theme_update_url();
  }
}

void SetCurrentThemeFromThemeSpecificsIfNecessary(
    const sync_pb::ThemeSpecifics& theme_specifics, Profile* profile) {
  DCHECK(profile);
  sync_pb::ThemeSpecifics old_theme_specifics;
  GetThemeSpecificsFromCurrentTheme(profile, &old_theme_specifics);
  if (!AreThemeSpecificsEqual(old_theme_specifics, theme_specifics)) {
    SetCurrentThemeFromThemeSpecifics(theme_specifics, profile);
  }
}

}  // namespace browser_sync
