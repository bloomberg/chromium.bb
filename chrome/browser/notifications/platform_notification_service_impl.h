// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_
#define CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_

#include "base/memory/singleton.h"
#include "content/public/browser/platform_notification_service.h"

// The platform notification service is the profile-agnostic entry point through
// which Web Notifications can be controlled.
class PlatformNotificationServiceImpl
    : public content::PlatformNotificationService {
 public:
  // Returns the active instance of the service in the browser process. Safe to
  // be called from any thread.
  static PlatformNotificationServiceImpl* GetInstance();

  // content::PlatformNotificationService implementation.
  blink::WebNotificationPermission CheckPermission(
      content::ResourceContext* resource_context,
      const GURL& origin,
      int render_process_id) override;;
  void DisplayNotification(
      content::BrowserContext* browser_context,
      const content::ShowDesktopNotificationHostMsgParams& params,
      scoped_ptr<content::DesktopNotificationDelegate> delegate,
      int render_process_id,
      base::Closure* cancel_callback) override;

 private:
  friend struct DefaultSingletonTraits<PlatformNotificationServiceImpl>;

  PlatformNotificationServiceImpl();
  ~PlatformNotificationServiceImpl() override;
};

#endif  // CHROME_BROWSER_NOTIFICATIONS_PLATFORM_NOTIFICATION_SERVICE_IMPL_H_
