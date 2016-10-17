// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NATIVE_NOTIFICATION_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NATIVE_NOTIFICATION_DISPLAY_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "chrome/browser/notifications/notification_common.h"
#include "chrome/browser/notifications/notification_display_service.h"

namespace base {
class NullableString16;
}

class Notification;
class NotificationHandler;
class NotificationPlatformBridge;
class Profile;

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
               const std::string& notification_id,
               const Notification& notification) override;
  void Close(NotificationCommon::Type notification_type,
             const std::string& notification_id) override;
  bool GetDisplayed(std::set<std::string>* notifications) const override;

  // Used by the notification bridge to propagate back events (click, close...).
  void ProcessNotificationOperation(NotificationCommon::Operation operation,
                                    NotificationCommon::Type notification_type,
                                    const std::string& origin,
                                    const std::string& notification_id,
                                    int action_index,
                                    const base::NullableString16& reply);

  // Registers an implementation object to handle notification operations
  // for |notification_type|.
  void AddNotificationHandler(NotificationCommon::Type notification_type,
                              std::unique_ptr<NotificationHandler> handler);

  // Removes an implementation added via |AddNotificationHandler|.
  void RemoveNotificationHandler(NotificationCommon::Type notification_type);

 private:
  NotificationHandler* GetNotificationHandler(
      NotificationCommon::Type notification_type);

  Profile* profile_;
  NotificationPlatformBridge* notification_bridge_;
  std::map<NotificationCommon::Type, std::unique_ptr<NotificationHandler>>
      notification_handlers_;

  DISALLOW_COPY_AND_ASSIGN(NativeNotificationDisplayService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NATIVE_NOTIFICATION_DISPLAY_SERVICE_H_
