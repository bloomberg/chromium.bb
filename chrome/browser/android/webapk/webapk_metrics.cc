// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/android/webapk/chrome_webapk_host.h"
#include "chrome/browser/installable/installable_metrics.h"

namespace webapk {

const char kInstallDurationHistogram[] = "WebApk.Install.InstallDuration";
const char kInstallEventHistogram[] = "WebApk.Install.InstallEvent";
const char kInstallSourceHistogram[] = "WebApk.Install.InstallSource";

void TrackRequestTokenDuration(base::TimeDelta delta) {
  UMA_HISTOGRAM_TIMES("WebApk.Install.RequestTokenDuration", delta);
}

void TrackInstallDuration(base::TimeDelta delta) {
  UMA_HISTOGRAM_MEDIUM_TIMES(kInstallDurationHistogram, delta);
}

void TrackInstallEvent(InstallEvent event) {
  UMA_HISTOGRAM_ENUMERATION(kInstallEventHistogram, event, INSTALL_EVENT_MAX);
}

void TrackInstallSource(WebAppInstallSource event) {
  webapk::InstallSource source;
  switch (event) {
    case WebAppInstallSource::AUTOMATIC_PROMPT:
    case WebAppInstallSource::API:
      source = InstallSource::INSTALL_SOURCE_BANNER;
      break;
    case WebAppInstallSource::MENU:
      source = InstallSource::INSTALL_SOURCE_MENU;
    case WebAppInstallSource::COUNT:
      NOTREACHED();
      return;
  }

  UMA_HISTOGRAM_ENUMERATION(kInstallSourceHistogram, source,
                            INSTALL_SOURCE_MAX);
  InstallableMetrics::TrackInstallSource(event);
}

}  // namespace webapk
