// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_METRICS_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_METRICS_H_

#include "chrome/browser/installable/installable_metrics.h"

namespace base {
class TimeDelta;
}

namespace webapk {

// Keep these enums up to date with tools/metrics/histograms/histograms.xml.
// Events for WebAPKs installation flow. The sum of InstallEvent histogram
// is the total number of times that a WebAPK infobar was triggered.
enum InstallEvent {
  // The user did not interact with the infobar.
  INFOBAR_IGNORED,
  // The infobar with the "Add-to-Homescreen" button is dismissed before the
  // installation started. "Dismiss" means the user closes the infobar by
  // clicking the "X" button.
  INFOBAR_DISMISSED_BEFORE_INSTALLATION,
  // The infobar with the "Adding" button is dismissed during installation.
  INFOBAR_DISMISSED_DURING_INSTALLATION,
  INSTALL_COMPLETED,
  INSTALL_FAILED,
  INSTALL_EVENT_MAX,
};

// The ways in which WebAPK installation can be started.
// InstallSource is deprecated in favor of WebappInstallSource, which tracks
// install sources for both desktop and Android.  If a new element is added to
// InstallSource, it should be mapped (in webapk::TrackInstallSource()) to an
// element in WebappInstallSource.
// TODO(crbug.com/790788): Once Webapp.Install.InstallSource contains enough
// historical data for Android, remove the Android-specific metric and use the
// general metric instead.
enum InstallSource {
  INSTALL_SOURCE_BANNER,
  INSTALL_SOURCE_MENU,
  INSTALL_SOURCE_MAX,
};

void TrackRequestTokenDuration(base::TimeDelta delta);
void TrackInstallDuration(base::TimeDelta delta);
void TrackInstallEvent(InstallEvent event);
void TrackInstallSource(WebappInstallSource event);

};  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_METRICS_H_
