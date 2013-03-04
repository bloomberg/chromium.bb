// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_launcher.h"

#include "apps/pref_names.h"
#include "apps/switches.h"
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
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShowAppListShortcut)) {
    return APP_LAUNCHER_ENABLED;
  }

#if defined(USE_ASH)
  if (chrome::GetActiveDesktop() == chrome::HOST_DESKTOP_TYPE_ASH)
    return APP_LAUNCHER_ENABLED;
#endif

  if (!BrowserDistribution::GetDistribution()->AppHostIsSupported())
    return APP_LAUNCHER_DISABLED;

  return APP_LAUNCHER_UNKNOWN;
#endif
}

#if defined(OS_WIN)
void UpdatePrefAndCallCallbackOnUI(
    bool result,
    const OnAppLauncherEnabledCompleted& completion_callback) {
  PrefService* prefs = g_browser_process->local_state();
  prefs->SetBoolean(prefs::kAppLauncherIsEnabled, result);
  completion_callback.Run(result);
}

void IsAppLauncherInstalledOnBlockingPool(
    const OnAppLauncherEnabledCompleted& completion_callback) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  bool result = chrome_launcher_support::IsAppLauncherPresent();
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
      base::Bind(UpdatePrefAndCallCallbackOnUI, result, completion_callback));
}
#endif

}  // namespace

bool MaybeIsAppLauncherEnabled() {
  return SynchronousAppLauncherChecks() == APP_LAUNCHER_ENABLED;
}

void GetIsAppLauncherEnabled(
    const OnAppLauncherEnabledCompleted& completion_callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  AppLauncherState state = SynchronousAppLauncherChecks();

  if (state != APP_LAUNCHER_UNKNOWN) {
    bool is_enabled = state == APP_LAUNCHER_ENABLED;
    PrefService* prefs = g_browser_process->local_state();
    prefs->SetBoolean(prefs::kAppLauncherIsEnabled, is_enabled);
    completion_callback.Run(is_enabled);
    return;
  }

#if defined(OS_WIN)
  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&IsAppLauncherInstalledOnBlockingPool,
                 completion_callback));
#else
  // SynchronousAppLauncherChecks() never returns APP_LAUNCHER_UNKNOWN on
  // !defined(OS_WIN), so this path is never reached.
  NOTREACHED();
#endif
}

bool WasAppLauncherEnabled() {
  PrefService* prefs = g_browser_process->local_state();
  // In some tests, the prefs aren't initialised.
  if (!prefs)
    return SynchronousAppLauncherChecks() == APP_LAUNCHER_ENABLED;
  return prefs->GetBoolean(prefs::kAppLauncherIsEnabled);
}

}  // namespace apps
