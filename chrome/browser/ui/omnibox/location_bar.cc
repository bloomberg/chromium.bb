// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/location_bar.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_handlers/settings_overrides_handler.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/permissions/permissions_data.h"


LocationBar::LocationBar(Profile* profile) : profile_(profile) {
}

LocationBar::~LocationBar() {
}

bool LocationBar::IsBookmarkStarHiddenByExtension() const {
  const ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  // Extension service may be NULL during unit test execution.
  if (!extension_service)
    return false;

  const extensions::ExtensionSet* extension_set =
      extension_service->extensions();
  for (extensions::ExtensionSet::const_iterator i = extension_set->begin();
       i != extension_set->end(); ++i) {
    using extensions::SettingsOverrides;
    const SettingsOverrides* settings_overrides =
        SettingsOverrides::Get(i->get());
    if (settings_overrides &&
        SettingsOverrides::RemovesBookmarkButton(*settings_overrides) &&
        (extensions::PermissionsData::HasAPIPermission(
            *i,
            extensions::APIPermission::kBookmarkManagerPrivate) ||
         extensions::FeatureSwitch::enable_override_bookmarks_ui()->
             IsEnabled()))
      return true;
  }

  return false;
}
