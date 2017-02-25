// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_METRICS_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_METRICS_H_

namespace base {
class TimeDelta;
}

enum class GooglePlayInstallState;

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
enum InstallSource {
  INSTALL_SOURCE_BANNER,
  INSTALL_SOURCE_MENU,
  INSTALL_SOURCE_MAX,
};

// The ways in which the WebAPK infobar can be shown.
enum InfoBarShown {
  WEBAPK_INFOBAR_SHOWN_FROM_BANNER,
  WEBAPK_INFOBAR_SHOWN_FROM_MENU,
  WEBAPK_INFOBAR_SHOWN_MAX,
};

// User actions after a WebAPK is installed.
enum UserAction {
  // TODO(hanxi|zpeng): Records the first two user actions after
  // crbug.com/638614 is fixed.
  // Launch a previously installed WebAPK since the WebAPK has been installed on
  // the device before.
  USER_ACTION_OPEN,
  USER_ACTION_OPEN_DISMISS,
  // Open a newly installed WebAPK via a successful installation.
  USER_ACTION_INSTALLED_OPEN,
  USER_ACTION_INSTALLED_OPEN_DISMISS,
  USER_ACTION_MAX,
};

void TrackInstallDuration(base::TimeDelta delta);
void TrackInstallEvent(InstallEvent event);
void TrackInstallSource(InstallSource event);
void TrackInstallInfoBarShown(InfoBarShown event);
void TrackUserAction(UserAction event);

// On web app and WebAPK installation records whether a WebAPK could be
// installed via the Google Play flow. If not, records why the WebAPK could not
// be installed via the Google Play flow (and a web app was added to the
// homescreen instead).
// Warning: This metric is recorded whenever a site is added to the homescreeen
// as a web app, not just for sites with a WebAPK compatible Web Manifest.
void TrackGooglePlayInstallState(GooglePlayInstallState state);

};  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_METRICS_H_
