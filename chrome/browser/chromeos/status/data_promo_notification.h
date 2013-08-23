// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_STATUS_DATA_PROMO_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_STATUS_DATA_PROMO_NOTIFICATION_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/network_state_handler_observer.h"

class PrefRegistrySimple;

namespace views {
class View;
}

namespace chromeos {

// This class is responsible for triggering cellular network related
// notifications, specifically:
// * "Chrome will use mobile data..." when Cellular is the Default network
//   for the first time.
// * Data Promotion notifications when available / appropriate.
class DataPromoNotification : public NetworkStateHandlerObserver {
 public:
  DataPromoNotification();
  virtual ~DataPromoNotification();

  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // NetworkStateHandlerObserver
  virtual void NetworkPropertiesUpdated(const NetworkState* network) OVERRIDE;
  virtual void DefaultNetworkChanged(const NetworkState* network) OVERRIDE;

  // Shows 3G promo notification if needed.
  void ShowOptionalMobileDataPromoNotification();

  // True if check for promo needs to be done, otherwise ignore it for the
  // current session.
  bool check_for_promo_;

  // Factory for delaying showing promo notification.
  base::WeakPtrFactory<DataPromoNotification> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataPromoNotification);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_STATUS_DATA_PROMO_NOTIFICATION_H_
