// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_list_util.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

bool IsAppLauncherEnabled() {
#if !BUILDFLAG(ENABLE_APP_LIST)
  return false;
#elif defined(OS_CHROMEOS) || defined(USE_ASH)
  return true;
#else
  PrefService* prefs = g_browser_process->local_state();
  // In some tests, the prefs aren't initialised.
  return prefs && prefs->GetBoolean(prefs::kAppLauncherHasBeenEnabled);
#endif
}

bool ShouldShowAppLauncherPromo() {
  // Never promote. TODO(tapted): Delete this function and supporting code.
  return false;
}
