// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "chrome/browser/android/webapk/chrome_webapk_host.h"

namespace webapk {

const char kGooglePlayInstallState[] = "WebApk.Install.GooglePlayInstallState";
const char kInstallDurationHistogram[] = "WebApk.Install.InstallDuration";
const char kInstallEventHistogram[] = "WebApk.Install.InstallEvent";
const char kInstallSourceHistogram[] = "WebApk.Install.InstallSource";
const char kInfoBarShownHistogram[] = "WebApk.Install.InfoBarShown";
const char kUserActionHistogram[] = "WebApk.Install.UserAction";

void TrackInstallDuration(base::TimeDelta delta) {
  UMA_HISTOGRAM_MEDIUM_TIMES(kInstallDurationHistogram, delta);
}

void TrackInstallEvent(InstallEvent event) {
  UMA_HISTOGRAM_ENUMERATION(kInstallEventHistogram, event, INSTALL_EVENT_MAX);
}

void TrackInstallSource(InstallSource event) {
  UMA_HISTOGRAM_ENUMERATION(kInstallSourceHistogram, event, INSTALL_SOURCE_MAX);
}

void TrackInstallInfoBarShown(InfoBarShown event) {
  UMA_HISTOGRAM_ENUMERATION(kInfoBarShownHistogram, event,
                            WEBAPK_INFOBAR_SHOWN_MAX);
}

void TrackUserAction(UserAction event) {
  UMA_HISTOGRAM_ENUMERATION(kUserActionHistogram, event, USER_ACTION_MAX);
}

void TrackGooglePlayInstallState(GooglePlayInstallState state) {
  UMA_HISTOGRAM_ENUMERATION(kGooglePlayInstallState, static_cast<int>(state),
                            static_cast<int>(GooglePlayInstallState::MAX));
}

}  // namespace webapk
