// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_TETHER_NOTIFICATION_PRESENTER_H_
#define CHROME_BROWSER_CHROMEOS_NET_TETHER_NOTIFICATION_PRESENTER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chromeos/components/tether/notification_presenter.h"
#include "chromeos/network/network_connect.h"
#include "components/cryptauth/remote_device.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/message_center/notification.h"

namespace message_center {
class MessageCenter;
class Notification;
}  // namespace message_center

namespace chromeos {

namespace tether {

// Produces notifications associated with CrOS tether network events and alerts
// observers about interactions with those notifications.
class TetherNotificationPresenter
    : public NotificationPresenter,
      public message_center::MessageCenterObserver {
 public:
  // Caller must ensure that |message_center| amd |network_connect| outlive
  // this instance.
  TetherNotificationPresenter(message_center::MessageCenter* message_center,
                              NetworkConnect* network_connect);
  ~TetherNotificationPresenter() override;

  // NotificationPresenter:
  void NotifyPotentialHotspotNearby(
      const cryptauth::RemoteDevice& remote_device) override;
  void NotifyMultiplePotentialHotspotsNearby() override;
  void RemovePotentialHotspotNotification() override;
  void NotifyConnectionToHostFailed() override;
  void RemoveConnectionToHostFailedNotification() override;

  // message_center::MessageCenterObserver:
  void OnNotificationClicked(const std::string& notification_id) override;
  void OnNotificationButtonClicked(const std::string& notification_id,
                                   int button_index) override;

 private:
  friend class TetherNotificationPresenterTest;

  static const char kTetherNotifierId[];
  static const char kPotentialHotspotNotificationId[];
  static const char kActiveHostNotificationId[];

  static std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& id,
      const base::string16& title,
      const base::string16& message);
  static std::unique_ptr<message_center::Notification> CreateNotification(
      const std::string& id,
      const base::string16& title,
      const base::string16& message,
      const message_center::RichNotificationData rich_notification_data);

  void ShowNotification(
      std::unique_ptr<message_center::Notification> notification);

  message_center::MessageCenter* message_center_;
  NetworkConnect* network_connect_;

  cryptauth::RemoteDevice hotspot_nearby_device_;
  base::WeakPtrFactory<TetherNotificationPresenter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TetherNotificationPresenter);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_TETHER_NOTIFICATION_PRESENTER_H_
