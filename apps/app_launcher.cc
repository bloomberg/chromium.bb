// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_launcher.h"

#include "apps/pref_names.h"
#include "base/command_line.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/host_desktop.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/browser_distribution.h"
#endif

namespace apps {

namespace {

enum AppLauncherState {
  APP_LAUNCHER_UNKNOWN,
  APP_LAUNCHER_ENABLED,
  APP_LAUNCHER_DISABLED,
};

AppLauncherState SynchronousAppLauncherChecks() {
#if defined(USE_ASH) && !defined(OS_WIN)
  return APP_LAUNCHER_ENABLED;
#elif !defined(OS_WIN)
  return APP_LAUNCHER_DISABLED;
#else
#if defined(USE_ASH)
  if (chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_ASH)
    return APP_LAUNCHER_ENABLED;
#endif
  PrefService* prefs = g_browser_process->local_state();
  // In some tests, the prefs aren't initialised.
  if (!prefs)
    return APP_LAUNCHER_UNKNOWN;
  return prefs->GetBoolean(prefs::kAppLauncherHasBeenEnabled) ?
      APP_LAUNCHER_ENABLED : APP_LAUNCHER_DISABLED;
#endif
}

}  // namespace

bool MaybeIsAppLauncherEnabled() {
  return SynchronousAppLauncherChecks() == APP_LAUNCHER_ENABLED;
}

void GetIsAppLauncherEnabled(
    const OnAppLauncherEnabledCompleted& completion_callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  AppLauncherState state = SynchronousAppLauncherChecks();

  if (state != APP_LAUNCHER_UNKNOWN) {
    completion_callback.Run(state == APP_LAUNCHER_ENABLED);
    return;
  }
  NOTREACHED();
}

bool WasAppLauncherEnabled() {
  return SynchronousAppLauncherChecks() == APP_LAUNCHER_ENABLED;
}

}  // namespace apps
