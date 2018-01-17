// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATION_PLATFORM_BRIDGE_WIN_H_

#include <windows.ui.notifications.h>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
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
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinTest, Suppress);
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinUITest, GetDisplayed);
  FRIEND_TEST_ALL_PREFIXES(NotificationPlatformBridgeWinUITest, HandleEvent);

  // Simulates a click/dismiss event. Only for use in testing.
  // Note: Ownership of |notification| and |args| is retained by the caller.
  void ForwardHandleEventForTesting(
      NotificationCommon::Operation operation,
      ABI::Windows::UI::Notifications::IToastNotification* notification,
      ABI::Windows::UI::Notifications::IToastActivatedEventArgs* args,
      const base::Optional<bool>& by_user);

  // Initializes the displayed notification vector. Only for use in testing.
  void SetDisplayedNotificationsForTesting(
      std::vector<ABI::Windows::UI::Notifications::IToastNotification*>*
          notifications);

  // Takes an |encoded| string as input and decodes it, returning the values in
  // the out parameters. |encoded| and |notifiation_id| must be provided. Other
  // pointers can be nullptr. Returns true if successful, but false otherwise.
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
