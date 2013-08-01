// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_launcher.h"

#include "apps/field_trial_names.h"
#include "apps/pref_names.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/host_desktop.h"

namespace apps {

bool IsAppLauncherEnabled() {
#if !defined(ENABLE_APP_LIST)
  return false;

#elif defined(OS_CHROMEOS)
  return true;

#else  // defined(ENABLE_APP_LIST) && !defined(OS_CHROMEOS)
  if (chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_ASH)
    return true;

  PrefService* prefs = g_browser_process->local_state();
  // In some tests, the prefs aren't initialised.
  return prefs && prefs->GetBoolean(prefs::kAppLauncherHasBeenEnabled);
#endif
}

bool ShouldShowAppLauncherPromo() {
  PrefService* local_state = g_browser_process->local_state();
  // In some tests, the prefs aren't initialised.
  if (!local_state)
    return false;
  std::string app_launcher_promo_group_name =
      base::FieldTrialList::FindFullName(apps::kLauncherPromoTrialName);
  return !IsAppLauncherEnabled() &&
      local_state->GetBoolean(apps::prefs::kShowAppLauncherPromo) &&
      (app_launcher_promo_group_name == apps::kShowLauncherPromoOnceGroupName ||
       app_launcher_promo_group_name ==
          apps::kResetShowLauncherPromoPrefGroupName);
}

}  // namespace apps
