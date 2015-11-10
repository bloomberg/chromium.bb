// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_controller_impl.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/rappor/rappor_utils.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/background_sync_launcher_android.h"
#endif

BackgroundSyncControllerImpl::BackgroundSyncControllerImpl(Profile* profile)
    : profile_(profile) {}

BackgroundSyncControllerImpl::~BackgroundSyncControllerImpl() = default;

rappor::RapporService* BackgroundSyncControllerImpl::GetRapporService() {
  return g_browser_process->rappor_service();
}

void BackgroundSyncControllerImpl::NotifyBackgroundSyncRegistered(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(origin, origin.GetOrigin());

  if (profile_->IsOffTheRecord())
    return;

  rappor::SampleDomainAndRegistryFromGURL(
      GetRapporService(), "BackgroundSync.Register.Origin", origin);
}

void BackgroundSyncControllerImpl::RunInBackground(bool enabled,
                                                   int64_t min_ms) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (profile_->IsOffTheRecord())
    return;
#if defined(OS_ANDROID)
  BackgroundSyncLauncherAndroid::LaunchBrowserIfStopped(enabled, min_ms);
#else
// TODO(jkarlin): Use BackgroundModeManager to enter background mode. See
// https://crbug.com/484201.
#endif
}
