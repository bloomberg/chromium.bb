// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_NOTIFICATION_ARC_BOOT_ERROR_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_ARC_NOTIFICATION_ARC_BOOT_ERROR_NOTIFICATION_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "components/arc/arc_service.h"

namespace arc {

// Watches for ARC boot errors and show notifications.
class ArcBootErrorNotification : public ArcService,
                                 public ArcSessionManager::Observer {
 public:
  explicit ArcBootErrorNotification(ArcBridgeService* bridge_service);
  ~ArcBootErrorNotification() override;

  // ArcSessionManager::Observer:
  void OnArcSessionStopped(ArcStopReason reason) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcBootErrorNotification);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_NOTIFICATION_ARC_BOOT_ERROR_NOTIFICATION_H_
