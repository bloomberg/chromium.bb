// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NATIVE_NOTIFICATION_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NATIVE_NOTIFICATION_DISPLAY_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"

class MessageCenterDisplayService;
class NotificationPlatformBridge;
class Profile;

namespace message_center {
class Notification;
}

// A class to display and interact with notifications in native notification
// centers on platforms that support it.
class NativeNotificationDisplayService : public NotificationDisplayService {
 public:
  NativeNotificationDisplayService(
      Profile* profile,
      NotificationPlatformBridge* notification_bridge);
  ~NativeNotificationDisplayService() override;

  // NotificationDisplayService implementation.
  void Display(NotificationCommon::Type notification_type,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(NotificationCommon::Type notification_type,
             const std::string& notification_id) override;
  void GetDisplayed(const DisplayedNotificationsCallback& callback) override;

 private:
  // Called by |notification_bridge_| when it is finished
  // initializing.  |success| indicates it is ready to be used.
  void OnNotificationPlatformBridgeReady(bool success);

  bool ShouldUsePlatformBridge(NotificationCommon::Type notification_type);

  Profile* profile_;

  NotificationPlatformBridge* notification_bridge_;
  // Indicates if |notification_bridge_| is ready to be used.
  bool notification_bridge_ready_;

  // MessageCenterDisplayService to fallback on if initialization of
  // |notification_bridge_| failed.
  std::unique_ptr<MessageCenterDisplayService> message_center_display_service_;

  // Tasks that need to be run once we have the initialization status
  // for |notification_bridge_|.
  base::queue<base::OnceClosure> actions_;

  base::WeakPtrFactory<NativeNotificationDisplayService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NativeNotificationDisplayService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NATIVE_NOTIFICATION_DISPLAY_SERVICE_H_
