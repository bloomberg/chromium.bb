// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"

namespace chromeos {

class NetworkState;

class NetworkPortalNotificationController {
 public:
  static const char kNotificationId[];

  NetworkPortalNotificationController();
  virtual ~NetworkPortalNotificationController();

  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state);

 private:
  // Last network path for which notification was displayed.
  std::string last_network_path_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalNotificationController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_NOTIFICATION_CONTROLLER_H_
