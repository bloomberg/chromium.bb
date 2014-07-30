// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_SHELL_NOTIFICATION_MANAGER_H_
#define CONTENT_SHELL_BROWSER_SHELL_NOTIFICATION_MANAGER_H_

#include <map>

#include "base/callback.h"
#include "third_party/WebKit/public/platform/WebNotificationPermission.h"
#include "url/gurl.h"

namespace content {

// Responsible for tracking active notifications and allowed origins for the
// Web Notification API when running layout tests. The methods in this class
// should only be used on the IO thread.
class ShellNotificationManager {
 public:
  ShellNotificationManager();
  ~ShellNotificationManager();

  // Checks whether |origin| has permission to display notifications in tests.
  blink::WebNotificationPermission CheckPermission(
      const GURL& origin);

  // Requests permission for |origin| to display notifications in layout tests.
  void RequestPermission(
      const GURL& origin,
      const base::Callback<void(blink::WebNotificationPermission)>& callback);

  // Sets the permission to display notifications for |origin| to |permission|.
  void SetPermission(const GURL& origin,
                     blink::WebNotificationPermission permission);

  // Clears the currently granted permissions.
  void ClearPermissions();

 private:
  typedef std::map<GURL, blink::WebNotificationPermission>
      NotificationPermissionMap;
  NotificationPermissionMap permission_map_;

  DISALLOW_COPY_AND_ASSIGN(ShellNotificationManager);
};

}  // content

#endif  // CONTENT_SHELL_BROWSER_SHELL_NOTIFICATION_MANAGER_H_
