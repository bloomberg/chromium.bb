// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NOTIFICATION_MANAGER_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NOTIFICATION_MANAGER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/platform_notification_service.h"
#include "content/public/common/permission_status.mojom.h"
#include "content/public/common/platform_notification_data.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationPermission.h"
#include "url/gurl.h"

namespace content {

class DesktopNotificationDelegate;
struct PlatformNotificationData;

// Responsible for tracking active notifications and allowed origins for the
// Web Notification API when running layout tests.
class LayoutTestNotificationManager : public PlatformNotificationService {
 public:
  LayoutTestNotificationManager();
  ~LayoutTestNotificationManager() override;

  // Requests permission for |origin| to display notifications in layout tests.
  // Must be called on the IO thread.
  // Returns whether the permission is granted.
  PermissionStatus RequestPermission(const GURL& origin);

  // Checks if |origin| has permission to display notifications. May be called
  // on both the IO and the UI threads.
  blink::WebNotificationPermission CheckPermission(const GURL& origin);

  // Similar to CheckPermission() above but returns a PermissionStatus object.
  PermissionStatus GetPermissionStatus(const GURL& origin);

  // Sets the permission to display notifications for |origin| to |permission|.
  // Must be called on the IO thread.
  void SetPermission(const GURL& origin,
                     blink::WebNotificationPermission permission);

  // Clears the currently granted permissions. Must be called on the IO thread.
  void ClearPermissions();

  // Simulates a click on the notification titled |title|. Must be called on the
  // UI thread.
  void SimulateClick(const std::string& title);

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
                           const SkBitmap& icon,
                           const PlatformNotificationData& notification_data,
                           scoped_ptr<DesktopNotificationDelegate> delegate,
                           base::Closure* cancel_callback) override;
  void DisplayPersistentNotification(
      BrowserContext* browser_context,
      int64 service_worker_registration_id,
      const GURL& origin,
      const SkBitmap& icon,
      const PlatformNotificationData& notification_data) override;
  void ClosePersistentNotification(
      BrowserContext* browser_context,
      const std::string& persistent_notification_id) override;

 private:
  // Closes the notification titled |title|. Must be called on the UI thread.
  void Close(const std::string& title);

  // Fakes replacing the notification identified by |params| when it has a tag
  // and a previous notification has been displayed using the same tag. All
  // notifications, both page and persistent ones, will be considered for this.
  void ReplaceNotificationIfNeeded(
      const PlatformNotificationData& notification_data);

  // Structure to represent the information of a persistent notification.
  struct PersistentNotification {
    PersistentNotification();

    BrowserContext* browser_context;
    GURL origin;
    int64 service_worker_registration_id;
    PlatformNotificationData notification_data;
    std::string persistent_id;
  };

  std::map<GURL, blink::WebNotificationPermission> permission_map_;
  base::Lock permission_lock_;

  std::map<std::string, DesktopNotificationDelegate*> page_notifications_;
  std::map<std::string, PersistentNotification> persistent_notifications_;

  std::map<std::string, std::string> replacements_;

  base::WeakPtrFactory<LayoutTestNotificationManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestNotificationManager);
};

}  // content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NOTIFICATION_MANAGER_H_
