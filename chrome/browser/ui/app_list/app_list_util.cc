// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_util.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

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
#if defined(OS_WIN)
  PrefService* local_state = g_browser_process->local_state();
  // In some tests, the prefs aren't initialised.
  if (!local_state)
    return false;
  return !IsAppLauncherEnabled() &&
      local_state->GetBoolean(prefs::kShowAppLauncherPromo);
#else
  return false;
#endif
}  // namespace apps
