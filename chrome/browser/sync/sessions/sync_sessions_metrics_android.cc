// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/metrics/field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync_sessions/sessions_sync_manager.h"
#include "components/sync_sessions/sync_sessions_metrics.h"
#include "jni/SyncSessionsMetrics_jni.h"

// static
void RecordYoungestForeignTabAgeOnNTP(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller) {
  // Unlike other platforms, Android typically disables session invalidations to
  // conserve battery. This means that the foreign tab data may be quite stale.
  // This would drastically distort the metric we want to emit here, however the
  // revisit experiement disables said optimization, allowing us to collect
  // valid data but at the cost of a much smaller sample size.
  const std::string group_name =
      base::FieldTrialList::FindFullName("PageRevisitInstrumentation");
  bool shouldInstrument = group_name == "Enabled";
  if (shouldInstrument) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    if (profile) {
      browser_sync::ProfileSyncService* sync =
          ProfileSyncServiceFactory::GetForProfile(profile);
      if (sync) {
        sync_sessions::SessionsSyncManager* sessions =
            static_cast<sync_sessions::SessionsSyncManager*>(
                sync->GetSessionsSyncableService());
        if (sessions) {
          sync_sessions::SyncSessionsMetrics::RecordYoungestForeignTabAgeOnNTP(
              sessions);
        }
      }
    }
  }
}
