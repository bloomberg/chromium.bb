// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/extensions/browser_actions_controller_prefs.h"

#include "chrome/common/pref_names.h"
#include "components/user_prefs/pref_registry_syncable.h"

void RegisterBrowserActionsControllerProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDoublePref(
      prefs::kBrowserActionContainerWidth,
      0,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}
