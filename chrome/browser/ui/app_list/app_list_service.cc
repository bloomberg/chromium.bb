// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_service.h"

#include "base/prefs/pref_registry_simple.h"
#include "chrome/common/pref_names.h"

// static
void AppListService::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kLastAppListLaunchPing, 0);
  registry->RegisterIntegerPref(prefs::kAppListLaunchCount, 0);
  registry->RegisterInt64Pref(prefs::kLastAppListAppLaunchPing, 0);
  registry->RegisterIntegerPref(prefs::kAppListAppLaunchCount, 0);
  registry->RegisterStringPref(prefs::kAppListProfile, std::string());
  registry->RegisterBooleanPref(prefs::kRestartWithAppList, false);
}
