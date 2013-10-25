// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_util.h"

#include "base/metrics/field_trial.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/pref_names.h"

namespace {

#if defined(ENABLE_APP_LIST)
// The field trial group name that enables showing the promo.
const char kShowLauncherPromoOnceGroupName[] = "ShowPromoUntilDismissed";

// The field trial group name that resets the pref to show the app launcher
// promo on every session startup.
const char kResetShowLauncherPromoPrefGroupName[] = "ResetShowPromoPref";

// The name of the field trial that controls showing the app launcher promo.
const char kLauncherPromoTrialName[] = "ShowAppLauncherPromo";
#endif  // defined(ENABLE_APP_LIST)

}  // namespace

void SetupShowAppLauncherPromoFieldTrial(PrefService* local_state) {
#if defined(ENABLE_APP_LIST)
  if (base::FieldTrialList::FindFullName(kLauncherPromoTrialName) ==
      kResetShowLauncherPromoPrefGroupName) {
    local_state->SetBoolean(prefs::kShowAppLauncherPromo, true);
  }
#endif
}

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
#if !defined(ENABLE_APP_LIST)
  return false;
#else
  PrefService* local_state = g_browser_process->local_state();
  // In some tests, the prefs aren't initialised.
  if (!local_state)
    return false;
  std::string app_launcher_promo_group_name =
      base::FieldTrialList::FindFullName(kLauncherPromoTrialName);
  return !IsAppLauncherEnabled() &&
      local_state->GetBoolean(prefs::kShowAppLauncherPromo) &&
      (app_launcher_promo_group_name == kShowLauncherPromoOnceGroupName ||
       app_launcher_promo_group_name == kResetShowLauncherPromoPrefGroupName);
#endif
}  // namespace apps
