// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/location_bar/location_bar.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_handlers/ui_overrides_handler.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/permissions/permissions_data.h"


LocationBar::LocationBar(Profile* profile) : profile_(profile) {
}

LocationBar::~LocationBar() {
}

bool LocationBar::IsBookmarkStarHiddenByExtension() const {
  const extensions::ExtensionSet& extension_set =
      extensions::ExtensionRegistry::Get(profile_)->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator i = extension_set.begin();
       i != extension_set.end(); ++i) {
    if (extensions::UIOverrides::RemovesBookmarkButton(i->get()) &&
        ((*i)->permissions_data()->HasAPIPermission(
             extensions::APIPermission::kBookmarkManagerPrivate) ||
         extensions::FeatureSwitch::enable_override_bookmarks_ui()
             ->IsEnabled()))
      return true;
  }

  return false;
}
