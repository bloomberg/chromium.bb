// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/notifications/notification_common.h"
#include "components/keyed_service/core/keyed_service.h"

class NotificationHandler;
class Profile;

namespace message_center {
class Notification;
}

// Profile-bound service that enables notifications to be displayed and
// interacted with on the user's screen, orthogonal of whether this
// functionality is provided by the browser or by the operating system. An
// instance can be retrieved through the NotificationDisplayServiceFactory.
//
// TODO(peter): Add a NotificationHandler mechanism for registering listeners.
class NotificationDisplayService : public KeyedService {
 public:
  using DisplayedNotificationsCallback =
      base::Callback<void(std::unique_ptr<std::set<std::string>>,
                          bool /* supports_synchronization */)>;

  static NotificationDisplayService* GetForProfile(Profile* profile);

  // Returns the NDS for system notifications which aren't tied to a particular
  // user. Currently only implemented on Chrome OS. TODO(estade): implement
  // elsewhere as needed.
  static NotificationDisplayService* GetForSystemNotifications();

  explicit NotificationDisplayService(Profile* profile);
  ~NotificationDisplayService() override;

  // Displays the |notification| identified by |notification_id|.
  virtual void Display(
      NotificationHandler::Type notification_type,
      const message_center::Notification& notification,
      std::unique_ptr<NotificationCommon::Metadata> metadata = nullptr) = 0;

  // Closes the notification identified by |notification_id|.
  virtual void Close(NotificationHandler::Type notification_type,
                     const std::string& notification_id) = 0;

  // Writes the ids of all currently displaying notifications and
  // invokes |callback| with the result once known.
  virtual void GetDisplayed(const DisplayedNotificationsCallback& callback) = 0;

  // Used to propagate back events originate from the user (click, close...).
  // The events are received and dispatched to the right consumer depending on
  // the type of notification. Consumers include, service workers, pages,
  // extensions...
  void ProcessNotificationOperation(NotificationCommon::Operation operation,
                                    NotificationHandler::Type notification_type,
                                    const GURL& origin,
                                    const std::string& notification_id,
                                    const base::Optional<int>& action_index,
                                    const base::Optional<base::string16>& reply,
                                    const base::Optional<bool>& by_user);

  // Returns the notification handler that was registered for the given type.
  // May return null.
  NotificationHandler* GetNotificationHandler(
      NotificationHandler::Type notification_type);

  // Registers an implementation object to handle notification operations
  // for |notification_type|.
  void AddNotificationHandler(NotificationHandler::Type notification_type,
                              std::unique_ptr<NotificationHandler> handler);

  // Removes an implementation object added via AddNotificationHandler.
  void RemoveNotificationHandler(NotificationHandler::Type notification_type);

 private:
  std::map<NotificationHandler::Type, std::unique_ptr<NotificationHandler>>
      notification_handlers_;
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(NotificationDisplayService);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_DISPLAY_SERVICE_H_
