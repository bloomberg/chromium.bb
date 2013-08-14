// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_DATA_PROMO_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_DATA_PROMO_NOTIFICATION_H_

#include "ash/system/chromeos/network/network_tray_delegate.h"
#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/network_state_handler_observer.h"

class PrefRegistrySimple;

namespace ash {
class NetworkTrayDelegate;
}

namespace views {
class View;
}

namespace chromeos {

// This class is responsible for triggering cellular network related
// notifications, specifically:
// * "Cellular Activated" when Cellular is activated and enabled for the
//   first time.
// * "Chrome will use mobile data..." when Cellular is the Default network
//   for the first time.
// * Data Promotion notifications when available / appropriate.
class DataPromoNotification : public NetworkStateHandlerObserver,
                              public ash::NetworkTrayDelegate {
 public:
  DataPromoNotification();
  virtual ~DataPromoNotification();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // NetworkStateHandlerObserver
  virtual void NetworkPropertiesUpdated(const NetworkState* network) OVERRIDE;
  virtual void DefaultNetworkChanged(const NetworkState* network) OVERRIDE;

  // ash::NetworkTrayDelegate
  virtual void NotificationLinkClicked(
      ash::NetworkObserver::MessageType message_type,
      size_t link_index) OVERRIDE;

  // Shows 3G promo notification if needed.
  void ShowOptionalMobileDataPromoNotification();

  // Updates the cellular activating state and checks for notification trigger.
  void UpdateCellularActivating();

  // Closes message bubble.
  void CloseNotification();

  // True if check for promo needs to be done, otherwise ignore it for the
  // current session.
  bool check_for_promo_;

  // Current carrier deal info URL.
  std::string deal_info_url_;

  // Current carrier deal top-up URL.
  std::string deal_topup_url_;

  // Internal state tracking.
  bool cellular_activating_;

  // Factory for delaying showing promo notification.
  base::WeakPtrFactory<DataPromoNotification> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataPromoNotification);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_DATA_PROMO_NOTIFICATION_H_
