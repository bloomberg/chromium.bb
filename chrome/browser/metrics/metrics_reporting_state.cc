// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_reporting_state.h"

#include "base/threading/thread_restrictions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/metrics_service.h"

bool ResolveMetricsReportingEnabled(bool enabled) {
  // GoogleUpdateSettings touches the disk from the UI thread. MetricsService
  // also calls GoogleUpdateSettings below. http://crbug/62626
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  GoogleUpdateSettings::SetCollectStatsConsent(enabled);
  bool update_pref = GoogleUpdateSettings::GetCollectStatsConsent();

  if (enabled != update_pref)
    DVLOG(1) << "Unable to set crash report status to " << enabled;

  // Only change the pref if GoogleUpdateSettings::GetCollectStatsConsent
  // succeeds.
  enabled = update_pref;

  MetricsService* metrics = g_browser_process->metrics_service();
  if (metrics) {
    if (enabled)
      metrics->Start();
    else
      metrics->Stop();
  }

  return enabled;
}
