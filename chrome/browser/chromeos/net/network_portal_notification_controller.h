// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_NOTIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_NOTIFICATION_CONTROLLER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"

namespace chromeos {

class NetworkState;
class NetworkPortalWebDialog;

class NetworkPortalNotificationController
    : public base::SupportsWeakPtr<NetworkPortalNotificationController> {
 public:
  // The values of these metrics are being used for UMA gathering, so it is
  // important that they don't change between releases.
  enum NotificationMetric {
    NOTIFICATION_METRIC_DISPLAYED = 0,

    // This value is no longer used by is still kept here just for
    // unify with histograms.xml.
    NOTIFICATION_METRIC_ERROR = 1,

    NOTIFICATION_METRIC_COUNT = 2
  };

  enum UserActionMetric {
    USER_ACTION_METRIC_CLICKED,
    USER_ACTION_METRIC_CLOSED,
    USER_ACTION_METRIC_IGNORED,
    USER_ACTION_METRIC_COUNT
  };

  static const char kNotificationId[];

  static const char kNotificationMetric[];
  static const char kUserActionMetric[];

  NetworkPortalNotificationController();
  virtual ~NetworkPortalNotificationController();

  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state);

  // Creates NetworkPortalWebDialog.
  void ShowDialog();

  // NULLifies reference to the active dialog.
  void OnDialogDestroyed(const NetworkPortalWebDialog* dialog);

 private:
  // Last network path for which notification was displayed.
  std::string last_network_path_;

  // Currently displayed authorization dialog, or NULL if none.
  NetworkPortalWebDialog* dialog_;

  DISALLOW_COPY_AND_ASSIGN(NetworkPortalNotificationController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_PORTAL_NOTIFICATION_CONTROLLER_H_
