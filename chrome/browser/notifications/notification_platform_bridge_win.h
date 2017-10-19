// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_

#include <windows.ui.notifications.h>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"
#include "url/gurl.h"

struct NotificationData;

// Implementation of the NotificationPlatformBridge for Windows 10 Anniversary
// Edition and beyond, delegating display of notifications to the Action Center.
class NotificationPlatformBridgeWin : public NotificationPlatformBridge {
 public:
  NotificationPlatformBridgeWin();
  ~NotificationPlatformBridgeWin() override;

  // NotificationPlatformBridge implementation.
  void Display(NotificationCommon::Type notification_type,
               const std::string& notification_id,
               const std::string& profile_id,
               bool incognito,
               const message_center::Notification& notification,
               std::unique_ptr<NotificationCommon::Metadata> metadata) override;
  void Close(const std::string& profile_id,
             const std::string& notification_id) override;
  void GetDisplayed(
      const std::string& profile_id,
      bool incognito,
      const GetDisplayedNotificationsCallback& callback) const override;
  void SetReadyCallback(NotificationBridgeReadyCallback callback) override;

 private:
  // Callbacks for toast events from Windows.
  HRESULT OnActivated(
      ABI::Windows::UI::Notifications::IToastNotification* notification,
      IInspectable* inspectable);
  HRESULT OnDismissed(
      ABI::Windows::UI::Notifications::IToastNotification* notification,
      ABI::Windows::UI::Notifications::IToastDismissedEventArgs* args);

  // Returns a notification with properties |notification_id|, |profile_id|,
  // |origin_url| and |incognito| if found in notifications_. Returns nullptr if
  // not found.
  NotificationData* FindNotificationData(const std::string& notification_id,
                                         const std::string& profile_id,
                                         const GURL& origin_url,
                                         bool incognito);

  // Whether the required functions from combase.dll have been loaded.
  bool com_functions_initialized_;

  // Stores the set of Notifications in a session.
  // A std::set<std::unique_ptr<T>> doesn't work well because e.g.,
  // std::set::erase(T) would require a std::unique_ptr<T> argument, so the data
  // would get double-destructed.
  template <typename T>
  using UnorderedUniqueSet = std::unordered_map<T*, std::unique_ptr<T>>;
  UnorderedUniqueSet<NotificationData> notifications_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeWin);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_
