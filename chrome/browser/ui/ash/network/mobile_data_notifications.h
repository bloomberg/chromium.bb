// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_NETWORK_MOBILE_DATA_NOTIFICATIONS_H_
#define CHROME_BROWSER_UI_ASH_NETWORK_MOBILE_DATA_NOTIFICATIONS_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/network_state_handler_observer.h"

// This class is responsible for triggering cellular network related
// notifications, specifically:
// * "Chrome will use mobile data..." when cellular is the default network
//   for the first time in a session.
class MobileDataNotifications : public chromeos::NetworkStateHandlerObserver {
 public:
  MobileDataNotifications();
  ~MobileDataNotifications() override;

 private:
  // NetworkStateHandlerObserver
  void NetworkPropertiesUpdated(const chromeos::NetworkState* network) override;
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;

  // Shows a one-time mobile data usage warning.
  void ShowOptionalMobileDataNotification();

  DISALLOW_COPY_AND_ASSIGN(MobileDataNotifications);
};

#endif  // CHROME_BROWSER_UI_ASH_NETWORK_MOBILE_DATA_NOTIFICATIONS_H_
