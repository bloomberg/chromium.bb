// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/prefs.h"

#include "apps/app_launcher.h"
#include "apps/pref_names.h"
#include "base/prefs/pref_registry_simple.h"

namespace apps {

void RegisterPrefs(PrefRegistrySimple* registry) {
  // This pref is a cache of the value from the registry the last time it was
  // checked.
  //
  // During the pref initialization, if it is impossible to synchronously
  // determine whether the app launcher is enabled, assume it is disabled.
  // Anything that needs to know the absolute truth should call
  // GetIsAppLauncherEnabled().
  registry->RegisterBooleanPref(prefs::kAppLauncherIsEnabled,
                                MaybeIsAppLauncherEnabled());
#if defined(OS_WIN)
  registry->RegisterStringPref(prefs::kAppLaunchForMetroRestart, "");
  registry->RegisterStringPref(prefs::kAppLaunchForMetroRestartProfile, "");
#endif

  // Identifies whether we should show the app launcher promo or not.
  // Now that a field trial also controls the showing, so the promo won't show
  // unless the pref is set AND the field trial is set to a proper group.
  registry->RegisterBooleanPref(prefs::kShowAppLauncherPromo, true);
}

}  // namespace apps
