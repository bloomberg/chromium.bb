// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater_basic.h"

#include "base/strings/string16.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"

void VersionUpdaterBasic::CheckForUpdate(
    const StatusCallback& status_callback,
    const PromoteCallback&) {
  const Status status = UpgradeDetector::GetInstance()->notify_upgrade()
                            ? NEARLY_UPDATED
                            : DISABLED;
  status_callback.Run(status, 0, false, std::string(), 0, base::string16());
}

VersionUpdater* VersionUpdater::Create(content::WebContents* web_contents) {
  return new VersionUpdaterBasic;
}
