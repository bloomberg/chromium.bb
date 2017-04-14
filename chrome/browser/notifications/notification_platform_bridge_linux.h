// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_

#include <gio/gio.h>

#include <unordered_map>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/base/glib/scoped_gobject.h"

class NotificationPlatformBridgeLinux : public NotificationPlatformBridge,
                                        public content::NotificationObserver {
 public:
  explicit NotificationPlatformBridgeLinux(GDBusProxy* notification_proxy);

  ~NotificationPlatformBridgeLinux() override;

  // NotificationPlatformBridge:
  void Display(NotificationCommon::Type notification_type,
               const std::string& notification_id,
               const std::string& profile_id,
               bool is_incognito,
               const Notification& notification) override;
  void Close(const std::string& profile_id,
             const std::string& notification_id) override;
  void GetDisplayed(
      const std::string& profile_id,
      bool incognito,
      const GetDisplayedNotificationsCallback& callback) const override;

  // Called from NotifyCompleteReceiver().
  void NotifyCompleteInternal(gpointer user_data, GVariant* value);

 private:
  struct NotificationData;
  struct ResourceFiles;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // Sets up a task to call NotifyNow() after an IO thread writes the
  // necessary resource files.
  void Notify(const Notification& notification,
              NotificationData* data,
              GAsyncReadyCallback callback,
              gpointer user_data);

  // Makes the "Notify" call to D-Bus.
  void NotifyNow(const Notification& notification,
                 base::WeakPtr<NotificationData> data,
                 GAsyncReadyCallback callback,
                 gpointer user_data,
                 std::unique_ptr<ResourceFiles> resource_files);

  // Makes the "CloseNotification" call to D-Bus.
  void CloseNow(uint32_t dbus_id);

  void ForwardNotificationOperation(uint32_t dbus_id,
                                    NotificationCommon::Operation operation,
                                    int action_index);

  // GSignalReceiver: The function that GLib calls into for
  // ActionInvoked and NotificationClosed signals.
  CHROMEG_CALLBACK_3(NotificationPlatformBridgeLinux,
                     void,
                     GSignalReceiver,
                     GDBusProxy*,
                     const char*,
                     const char*,
                     GVariant*);

  NotificationData* FindNotificationData(const std::string& notification_id,
                                         const std::string& profile_id,
                                         bool is_incognito);

  NotificationData* FindNotificationData(uint32_t dbus_id);

  ScopedGObject<GDBusProxy> notification_proxy_;

  // Used to disconnect from "g-signal" during destruction.
  gulong proxy_signal_handler_ = 0;

  // A std::set<std::unique_ptr<T>> doesn't work well because
  // eg. std::set::erase(T) would require a std::unique_ptr<T>
  // argument, so the data would get double-destructed.
  template <typename T>
  using UnorderedUniqueSet = std::unordered_map<T*, std::unique_ptr<T>>;

  UnorderedUniqueSet<NotificationData> notifications_;

  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<NotificationPlatformBridgeLinux> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeLinux);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_LINUX_H_
