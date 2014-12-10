// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NOTIFICATION_MANAGER_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NOTIFICATION_MANAGER_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/platform_notification_service.h"
#include "third_party/WebKit/public/platform/WebNotificationPermission.h"
#include "url/gurl.h"

namespace content {

struct ShowDesktopNotificationHostMsgParams;
class DesktopNotificationDelegate;

// Responsible for tracking active notifications and allowed origins for the
// Web Notification API when running layout tests.
class LayoutTestNotificationManager : public PlatformNotificationService {
 public:
  LayoutTestNotificationManager();
  ~LayoutTestNotificationManager() override;

  // Requests permission for |origin| to display notifications in layout tests.
  // Must be called on the IO thread.
  // Returns whether the permission is granted.
  bool RequestPermission(const GURL& origin);

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
  blink::WebNotificationPermission CheckPermission(
      ResourceContext* resource_context,
      const GURL& origin,
      int render_process_id) override;
  void DisplayNotification(BrowserContext* browser_context,
                           const ShowDesktopNotificationHostMsgParams& params,
                           scoped_ptr<DesktopNotificationDelegate> delegate,
                           int render_process_id,
                           base::Closure* cancel_callback) override;

 private:
  // Closes the notification titled |title|. Must be called on the UI thread.
  void Close(const std::string& title);

  typedef std::map<GURL, blink::WebNotificationPermission>
      NotificationPermissionMap;
  NotificationPermissionMap permission_map_;

  std::map<std::string, DesktopNotificationDelegate*> notifications_;
  std::map<std::string, std::string> replacements_;

  base::WeakPtrFactory<LayoutTestNotificationManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(LayoutTestNotificationManager);
};

}  // content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_NOTIFICATION_MANAGER_H_
