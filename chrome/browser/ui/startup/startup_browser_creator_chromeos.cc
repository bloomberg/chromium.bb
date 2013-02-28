// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_browser_creator.h"

#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/chromeos/net/network_online_monitor.h"
#include "chrome/browser/chromeos/ui/app_launch_view.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace {

// Application install splash screen minimum show time in milliseconds.
const int kAppInstallSplashScreenMinTimeMS = 3000;

}  // namespace

// static
int64 StartupBrowserCreator::ShowAppInstallUI() {
  if (chrome::IsRunningInAppMode()) {
    chromeos::ShowAppLaunchSplashScreen();

    const int kMaxAllowedWaitMilliseconds = 5000;
    chromeos::NetworkOnlineMonitor online_waiter(kMaxAllowedWaitMilliseconds);
    online_waiter.Wait();
  }

  return base::TimeTicks::Now().ToInternalValue();
}

// static
void StartupBrowserCreator::HideAppInstallUI(
    int64 start_time) {
  if (!chrome::IsRunningInAppMode())
    return;

  int64 time_taken_ms = (base::TimeTicks::Now() -
      base::TimeTicks::FromInternalValue(start_time)).InMilliseconds();

  // Enforce that we show app install splash screen for some minimum abount
  // of time.
  if (time_taken_ms < kAppInstallSplashScreenMinTimeMS) {
    BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(chromeos::CloseAppLaunchSplashScreen),
        base::TimeDelta::FromMilliseconds(
            kAppInstallSplashScreenMinTimeMS - time_taken_ms));
    return;
  }
  chromeos::CloseAppLaunchSplashScreen();
}

