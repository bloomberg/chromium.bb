// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NOTIFICATION_MANAGER_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NOTIFICATION_MANAGER_H_

#include <stdint.h>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/platform_notification_service.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"
#include "url/gurl.h"

namespace base {
class NullableString16;
}

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
  // clicked. |reply| indicates the user reply, if any.
  // Must be called on the UI thread.
  void SimulateClick(const std::string& title,
                     int action_index,
                     const base::NullableString16& reply);

  // Simulates the closing a notification titled |title|. Must be called on
  // the UI thread.
  void SimulateClose(const std::string& title, bool by_user);

  // PlatformNotificationService implementation.
  blink::mojom::PermissionStatus CheckPermissionOnUIThread(
      BrowserContext* browser_context,
      const GURL& origin,
      int render_process_id) override;
  blink::mojom::PermissionStatus CheckPermissionOnIOThread(
      ResourceContext* resource_context,
      const GURL& origin,
      int render_process_id) override;
  void DisplayNotification(
      BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& origin,
      const PlatformNotificationData& notification_data,
      const NotificationResources& notification_resources,
      std::unique_ptr<DesktopNotificationDelegate> delegate,
      base::Closure* cancel_callback) override;
  void DisplayPersistentNotification(
      BrowserContext* browser_context,
      const std::string& notification_id,
      const GURL& service_worker_scope,
      const GURL& origin,
      const PlatformNotificationData& notification_data,
      const NotificationResources& notification_resources) override;
  void ClosePersistentNotification(BrowserContext* browser_context,
                                   const std::string& notification_id) override;
  bool GetDisplayedNotifications(
      BrowserContext* browser_context,
      std::set<std::string>* displayed_notifications) override;

 private:
  // Structure to represent the information of a persistent notification.
  struct PersistentNotification {
    BrowserContext* browser_context = nullptr;
    GURL origin;
  };

  // Closes the notification titled |title|. Must be called on the UI thread.
  void Close(const std::string& title);

  // Fakes replacing the notification identified by |notification_id|. Both
  // persistent and non-persistent notifications will be considered for this.
  void ReplaceNotificationIfNeeded(const std::string& notification_id);

  // Checks if |origin| has permission to display notifications. May be called
  // on both the IO and the UI threads.
  blink::mojom::PermissionStatus CheckPermission(const GURL& origin);

  std::unordered_map<std::string, PersistentNotification>
      persistent_notifications_;
  std::unordered_map<std::string, std::unique_ptr<DesktopNotificationDelegate>>
      non_persistent_notifications_;

  // Mapping of titles to notification ids giving test a usable identifier.
  std::unordered_map<std::string, std::string> notification_id_map_;

  base::WeakPtrFactory<LayoutTestNotificationManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestNotificationManager);
};

}  // content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NOTIFICATION_MANAGER_H_
