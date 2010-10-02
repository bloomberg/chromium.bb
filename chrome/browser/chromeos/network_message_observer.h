// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NETWORK_MESSAGE_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_NETWORK_MESSAGE_OBSERVER_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/notifications/system_notification.h"

class Profile;
namespace views {
class WindowDelegate;
}

namespace chromeos {

// We show nearing expiration notification when less than 30 minutes.
static const int kDataNearingExpirationSecs = 30 * 60;

// We show low data notification when less than 50MB.
static const int kDataLowDataBytes = 50 * 1024;

// The network message observer displays a system notification for network
// messages.

class NetworkMessageObserver : public NetworkLibrary::Observer {
 public:
  explicit NetworkMessageObserver(Profile* profile);
  virtual ~NetworkMessageObserver();

  typedef std::map<std::string, WifiNetwork> ServicePathWifiMap;
  typedef std::map<std::string, CellularNetwork> ServicePathCellularMap;
 private:
  virtual void CreateModalPopup(views::WindowDelegate* view);

  // NetworkLibrary::Observer implementation.
  virtual void NetworkChanged(NetworkLibrary* obj);
  virtual void CellularDataPlanChanged(const std::string& service_path,
                                       const CellularDataPlan& plan);

  bool initialized_;
  // Wifi networks by service path.
  ServicePathWifiMap wifi_networks_;
  // Cellular networks by service path.
  ServicePathCellularMap cellular_networks_;

  // Current connect celluar service path.
  std::string cellular_service_path_;
  // Last cellular data plan data.
  CellularDataPlan cellular_data_plan_;

  // Notification for connection errors
  SystemNotification notification_connection_error_;
  // Notification for showing low data warning
  SystemNotification notification_low_data_;
  // Notification for showing no data warning
  SystemNotification notification_no_data_;

  DISALLOW_COPY_AND_ASSIGN(NetworkMessageObserver);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NETWORK_MESSAGE_OBSERVER_H_

