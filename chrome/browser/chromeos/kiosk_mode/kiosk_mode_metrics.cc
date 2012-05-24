// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_metrics.h"

#include "base/metrics/histogram.h"

namespace {

const char kDemoUserSessionTimeMetric[] = "KioskMode.DemoUserSessionTime";
const char kDemoUserAppsLaunchedMetric[] = "KioskMode.DemoUserAppsLaunched";

}

namespace chromeos {

static KioskModeMetrics* kiosk_mode_settings = NULL;

// static
KioskModeMetrics* KioskModeMetrics::Get() {
  if (!kiosk_mode_settings)
    kiosk_mode_settings = new KioskModeMetrics();

  return kiosk_mode_settings;
}

void KioskModeMetrics::SessionStarted() {
  session_started_ = base::Time::Now();
}

void KioskModeMetrics::SessionEnded() {
  if (session_started_ == base::Time())
    return;

  // Session ended, time to report our collected metrics.
  UMA_HISTOGRAM_LONG_TIMES(kDemoUserSessionTimeMetric,
                           base::Time::Now() - session_started_);

  UMA_HISTOGRAM_COUNTS(kDemoUserAppsLaunchedMetric, apps_opened_);

  // We've consumed the session times, reset.
  session_started_ = base::Time();
}

void KioskModeMetrics::UserOpenedApp() {
  if (session_started_ == base::Time())
    return;
  ++apps_opened_;
}

KioskModeMetrics::KioskModeMetrics()
    : session_started_(base::Time()),
      apps_opened_(0) {
}

KioskModeMetrics::~KioskModeMetrics() {
}

}  // namespace chromeos
