// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace webapk {

const char kInstallEventHistogram[] = "WebApk.Install.InstallEvent";
const char kInstallSourceHistogram[] = "WebApk.Install.InstallSource";
const char kInfoBarShownHistogram[] = "WebApk.Install.InfoBarShown";
const char kUserActionHistogram[] = "WebApk.Install.UserAction";

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

}  // namespace webapk
