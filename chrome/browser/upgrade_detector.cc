// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector.h"

#include "base/file_version_info.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/version.h"
#include "chrome/app/chrome_version_info.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/installer/util/browser_distribution.h"

#if defined(OS_WIN)
#include "chrome/installer/util/install_util.h"
#endif

// TODO(finnur): For the stable channel we want to check daily and notify
// the user if more than 2 weeks have passed since the upgrade happened
// (without a reboot). For the dev channel however, I want quicker feedback
// on how the feature works so I'm checking every hour and notifying the
// user immediately.

// How often to check for an upgrade.
static int kCheckForUpgradeEveryMs = 60 * 60 * 1000;  // 1 hour.

// How long to wait before notifying the user about the upgrade.
static int kNotifyUserAfterMs = 0;

UpgradeDetector::UpgradeDetector()
    : upgrade_detected_(false),
      notify_upgrade_(false) {
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  detect_upgrade_timer_.Start(
      base::TimeDelta::FromMilliseconds(kCheckForUpgradeEveryMs),
      this, &UpgradeDetector::CheckForUpgrade);
#endif
}

UpgradeDetector::~UpgradeDetector() {
}

void UpgradeDetector::CheckForUpgrade() {
#if defined(OS_WIN)
  using installer::Version;

  // Get the version of the currently *installed* instance of Chrome,
  // which might be newer than the *running* instance if we have been
  // upgraded in the background.
  Version* installed_version = InstallUtil::GetChromeVersion(false);
  if (!installed_version) {
    // User level Chrome is not installed, check system level.
    installed_version = InstallUtil::GetChromeVersion(true);
  }

  // Get the version of the currently *running* instance of Chrome.
  scoped_ptr<FileVersionInfo> version(chrome_app::GetChromeVersionInfo());
  if (version.get() == NULL) {
    NOTREACHED() << L"Failed to get current file version";
    return;
  }
  scoped_ptr<Version> running_version(Version::GetVersionFromString(
      version->file_version()));

  if (installed_version->IsHigherThan(running_version.get())) {
    // Stop the recurring timer (that is checking for changes).
    detect_upgrade_timer_.Stop();

    upgrade_detected_ = true;

    NotificationService::current()->Notify(
        NotificationType::UPGRADE_DETECTED,
        Source<UpgradeDetector>(this),
        NotificationService::NoDetails());

    // Start the OneShot timer for notifying the user after a certain period.
    upgrade_notification_timer_.Start(
        base::TimeDelta::FromMilliseconds(kNotifyUserAfterMs),
        this, &UpgradeDetector::NotifyOnUpgrade);
  }
#else
  DCHECK(kNotifyUserAfterMs > 0);  // Avoid error: var defined but not used.
  NOTIMPLEMENTED();
#endif
}

void UpgradeDetector::NotifyOnUpgrade() {
  notify_upgrade_ = true;

  NotificationService::current()->Notify(
      NotificationType::UPGRADE_RECOMMENDED,
      Source<UpgradeDetector>(this),
      NotificationService::NoDetails());
}
