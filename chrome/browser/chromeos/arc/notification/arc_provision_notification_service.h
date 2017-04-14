// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_NOTIFICATION_ARC_PROVISION_NOTIFICATION_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ARC_NOTIFICATION_ARC_PROVISION_NOTIFICATION_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/chromeos/arc/arc_session_manager.h"
#include "components/arc/arc_service.h"

namespace arc {

// Watches for ARC provisioning status and displays a notification during
// provision when ARC opt-in flow happens silently due to configured policies.
class ArcProvisionNotificationService : public ArcService,
                                        public ArcSessionManager::Observer {
 public:
  // The delegate whose methods are used by the service for showing/hiding the
  // notifications. The delegate exists for unit testing purposes.
  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();
    virtual void ShowManagedProvisionNotification() = 0;
    virtual void RemoveManagedProvisionNotification() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  // Constructs with the default delegate implementation that uses message
  // center for showing the notifications.
  explicit ArcProvisionNotificationService(ArcBridgeService* bridge_service);

  ~ArcProvisionNotificationService() override;

  // Constructs an instance with the supplied delegate.
  static std::unique_ptr<ArcProvisionNotificationService> CreateForTesting(
      ArcBridgeService* bridge_service,
      std::unique_ptr<Delegate> delegate);

 private:
  // Constructs with the supplied delegate.
  ArcProvisionNotificationService(ArcBridgeService* bridge_service,
                                  std::unique_ptr<Delegate> delegate);

  // ArcSessionManager::Observer:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;
  void OnArcOptInManagementCheckStarted() override;
  void OnArcInitialStart() override;
  void OnArcSessionStopped(ArcStopReason stop_reason) override;
  void OnArcErrorShowRequested(ArcSupportHost::Error error) override;

  std::unique_ptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ArcProvisionNotificationService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_NOTIFICATION_ARC_PROVISION_NOTIFICATION_SERVICE_H_
