// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PLATFORM_NOTIFICATION_SERVICE_H_
#define CONTENT_PUBLIC_BROWSER_PLATFORM_NOTIFICATION_SERVICE_H_

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/common/show_desktop_notification_params.h"
#include "third_party/WebKit/public/platform/WebNotificationPermission.h"

class GURL;

namespace content {

class BrowserContext;
class DesktopNotificationDelegate;
class ResourceContext;

// The service using which notifications can be presented to the user. There
// should be a unique instance of the PlatformNotificationService depending
// on the browsing context being used.
class CONTENT_EXPORT PlatformNotificationService {
 public:
  virtual ~PlatformNotificationService() {}

  // Checks if |origin| has permission to display Web Notifications. This method
  // must be called on the IO thread.
  virtual blink::WebNotificationPermission CheckPermission(
      ResourceContext* resource_context,
      const GURL& origin,
      int render_process_id) = 0;

  // Displays the notification described in |params| to the user. A closure
  // through which the notification can be closed will be stored in the
  // |cancel_callback| argument. This method must be called on the UI thread.
  virtual void DisplayNotification(
      BrowserContext* browser_context,
      const ShowDesktopNotificationHostMsgParams& params,
      scoped_ptr<DesktopNotificationDelegate> delegate,
      int render_process_id,
      base::Closure* cancel_callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PLATFORM_NOTIFICATION_SERVICE_H_
