// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NOTIFICATION_MANAGER_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NOTIFICATION_MANAGER_H_

#include <stdint.h>
#include <map>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/platform_notification_service.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationPermission.h"
#include "url/gurl.h"

namespace content {

class DesktopNotificationDelegate;
struct NotificationResources;
struct PlatformNotificationData;

// Responsible for tracking active notifications and allowed origins for the
// Web Notification API when running layout tests.
class LayoutTestNotificationManager : public PlatformNotificationService {
 public:
  LayoutTestNotificationManager();
  ~LayoutTestNotificationManager() override;

  // Simulates a click on the notification titled |title|. |action_index|
  // indicates which action was clicked, or -1 if the main notification body was
  // clicked. Must be called on the UI thread.
  void SimulateClick(const std::string& title, int action_index);

  // PlatformNotificationService implementation.
  blink::WebNotificationPermission CheckPermissionOnUIThread(
      BrowserContext* browser_context,
      const GURL& origin,
      int render_process_id) override;
  blink::WebNotificationPermission CheckPermissionOnIOThread(
      ResourceContext* resource_context,
      const GURL& origin,
      int render_process_id) override;
  void DisplayNotification(BrowserContext* browser_context,
                           const GURL& origin,
                           const PlatformNotificationData& notification_data,
                           const NotificationResources& notification_resources,
                           scoped_ptr<DesktopNotificationDelegate> delegate,
                           base::Closure* cancel_callback) override;
  void DisplayPersistentNotification(
      BrowserContext* browser_context,
      int64_t persistent_notification_id,
      const GURL& origin,
      const PlatformNotificationData& notification_data,
      const NotificationResources& notification_resources) override;
  void ClosePersistentNotification(
      BrowserContext* browser_context,
      int64_t persistent_notification_id) override;
  bool GetDisplayedPersistentNotifications(
      BrowserContext* browser_context,
      std::set<std::string>* displayed_notifications) override;

 private:
  // Structure to represent the information of a persistent notification.
  struct PersistentNotification {
    BrowserContext* browser_context = nullptr;
    GURL origin;
    int64_t persistent_id = 0;
  };

  // Closes the notification titled |title|. Must be called on the UI thread.
  void Close(const std::string& title);

  // Fakes replacing the notification identified by |params| when it has a tag
  // and a previous notification has been displayed using the same tag. All
  // notifications, both page and persistent ones, will be considered for this.
  void ReplaceNotificationIfNeeded(
      const PlatformNotificationData& notification_data);

  // Checks if |origin| has permission to display notifications. May be called
  // on both the IO and the UI threads.
  blink::WebNotificationPermission CheckPermission(const GURL& origin);

  std::map<std::string, DesktopNotificationDelegate*> page_notifications_;
  std::map<std::string, PersistentNotification> persistent_notifications_;

  std::map<std::string, std::string> replacements_;

  base::WeakPtrFactory<LayoutTestNotificationManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestNotificationManager);
};

}  // content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NOTIFICATION_MANAGER_H_
