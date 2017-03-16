// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_NOTIFICATION_PRESENTER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_NOTIFICATION_PRESENTER_H_

#include <string>

#include "base/macros.h"
#include "chromeos/components/tether/notification_presenter.h"

namespace chromeos {

namespace tether {

class FakeNotificationPresenter : public NotificationPresenter {
 public:
  FakeNotificationPresenter();
  ~FakeNotificationPresenter() override;

  // If no "connection to host failed" dialog is visible, "" is returned.
  const std::string& connection_failed_notification_device_name() const {
    return connection_failed_notification_device_name_;
  }

  // NotificationPresenter:
  void NotifyConnectionToHostFailed(
      const std::string& host_device_name) override;
  void RemoveConnectionToHostFailedNotification() override;

 private:
  std::string connection_failed_notification_device_name_;

  DISALLOW_COPY_AND_ASSIGN(FakeNotificationPresenter);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_NOTIFICATION_PRESENTER_H_
