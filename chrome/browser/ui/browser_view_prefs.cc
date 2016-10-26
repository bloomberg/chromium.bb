// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_view_prefs.h"

#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include "ui/base/x/x11_util.h"  // nogncheck
#endif

namespace {

// DEPRECATED: this is replaced by kTabStripStackedLayout and exists for
// backwards compatibility.
// Old values: 0 = SHRINK (default), 1 = STACKED.
const char kTabStripLayoutType[] = "tab_strip_layout_type";

}  // namespace

namespace chrome {

void RegisterBrowserViewLocalPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTabStripLayoutType, 0);
  registry->RegisterBooleanPref(prefs::kTabStripStackedLayout, false);
}

void RegisterBrowserViewProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
#if !defined(OS_CHROMEOS) && defined(USE_X11)
  registry->RegisterBooleanPref(prefs::kUseCustomChromeFrame,
                                ui::GetCustomFramePrefDefault());
#endif

  registry->RegisterIntegerPref(
      prefs::kBackShortcutBubbleShownCount, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void MigrateBrowserTabStripPrefs(PrefService* prefs) {
  if (prefs->HasPrefPath(kTabStripLayoutType)) {
    prefs->SetBoolean(prefs::kTabStripStackedLayout,
                      prefs->GetInteger(kTabStripLayoutType) != 0);
    prefs->ClearPref(kTabStripLayoutType);
  }
}

}  // namespace chrome
