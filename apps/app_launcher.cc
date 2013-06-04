// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_launcher.h"

#include "apps/pref_names.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/host_desktop.h"

#if defined(OS_WIN)
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/browser_distribution.h"
#endif

namespace apps {

bool IsAppLauncherEnabled() {
#if defined(USE_ASH) && !defined(OS_WIN)
  return true;
#elif !defined(OS_WIN)
  return false;
#else
#if defined(USE_ASH)
  if (chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_ASH)
    return true;
#endif
  PrefService* prefs = g_browser_process->local_state();
  // In some tests, the prefs aren't initialised.
  if (!prefs)
    return false;
  return prefs->GetBoolean(prefs::kAppLauncherHasBeenEnabled);
#endif
}

}  // namespace apps
