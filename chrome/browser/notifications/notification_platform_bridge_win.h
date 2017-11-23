// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_

#include <windows.ui.notifications.h>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/notifications/notification_platform_bridge.h"

class NotificationPlatformBridgeWinImpl;
class NotificationTemplateBuilder;

// Implementation of the NotificationPlatformBridge for Windows 10 Anniversary
// Edition and beyond, delegating display of notifications to the Action Center.
class NotificationPlatformBridgeWin : public NotificationPlatformBridge {
 public:
  NotificationPlatformBridgeWin();
  ~NotificationPlatformBridgeWin() override;

  // NotificationPlatformBridge implementation.
  void Display(NotificationHandler::Type notification_type,
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
  friend class NotificationPlatformBridgeWinImpl;
  friend class NotificationPlatformBridgeWinTest;
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinTest, EncodeDecode);

  // Takes an |encoded| string as input and decodes it, returning the values in
  // the out parameters. Returns true if successful, but false otherwise.
  static bool DecodeTemplateId(const std::string& encoded,
                               NotificationHandler::Type* notification_type,
                               std::string* notification_id,
                               std::string* profile_id,
                               bool* incognito,
                               GURL* origin_url) WARN_UNUSED_RESULT;

  // Encodes a template ID string given the input parameters.
  static std::string EncodeTemplateId(
      NotificationHandler::Type notification_type,
      const std::string& notification_id,
      const std::string& profile_id,
      bool incognito,
      const GURL& origin_url);

  // Obtain an IToastNotification interface from a given XML (provided by the
  // NotificationTemplateBuilder). For testing use only.
  HRESULT GetToastNotificationForTesting(
      const message_center::Notification& notification,
      const NotificationTemplateBuilder& notification_template_builder,
      ABI::Windows::UI::Notifications::IToastNotification** toast_notification);

  void PostTaskToTaskRunnerThread(base::OnceClosure closure) const;

  scoped_refptr<NotificationPlatformBridgeWinImpl> impl_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(NotificationPlatformBridgeWin);
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_
