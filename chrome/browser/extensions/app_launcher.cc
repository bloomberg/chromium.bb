// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_launcher.h"

#include "base/command_line.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include "chrome/installer/launcher_support/chrome_launcher_support.h"
#include "chrome/installer/util/browser_distribution.h"
#endif

namespace extensions {

namespace {

#if defined(OS_WIN)
void IsAppLauncherInstalledOnBlockingPool(
    const OnAppLauncherEnabledCompleted& completion_callback) {
  DCHECK(content::BrowserThread::GetBlockingPool()->RunsTasksOnCurrentThread());
  bool result = chrome_launcher_support::IsAppLauncherPresent();
  content::BrowserThread::PostTask(content::BrowserThread::UI,
                                   FROM_HERE,
                                   base::Bind(completion_callback, result));
}
#endif

}  // namespace

void GetIsAppLauncherEnabled(
    const OnAppLauncherEnabledCompleted& completion_callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
#if defined(OS_CHROMEOS)
  completion_callback.Run(true);
#elif !defined(OS_WIN)
  completion_callback.Run(false);
#else
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kShowAppListShortcut)) {
    completion_callback.Run(true);
    return;
  }

  if (!BrowserDistribution::GetDistribution()->AppHostIsSupported()) {
    completion_callback.Run(false);
    return;
  }

  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(&IsAppLauncherInstalledOnBlockingPool,
                 completion_callback));
#endif
}

}  // namespace extensions
