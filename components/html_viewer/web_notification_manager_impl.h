// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_WEB_NOTIFICATION_MANAGER_IMPL_H_
#define COMPONENTS_HTML_VIEWER_WEB_NOTIFICATION_MANAGER_IMPL_H_

#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationManager.h"

namespace blink {
class WebSecurityOrigin;
}

namespace html_viewer {

// TODO(erg): This class is currently a stub; blink expects this object to
// exist, and several websites will trigger notifications these days.
class WebNotificationManagerImpl : public blink::WebNotificationManager {
 public:
  WebNotificationManagerImpl();
  ~WebNotificationManagerImpl() override;

  // blink::WebNotificationManager methods:
  void show(const blink::WebSecurityOrigin& origin,
            const blink::WebNotificationData& notification_data,
            blink::WebNotificationDelegate* delegate) override;
  void showPersistent(
      const blink::WebSecurityOrigin& origin,
      const blink::WebNotificationData& notification_data,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebNotificationShowCallbacks* callbacks) override;
  void getNotifications(
      const blink::WebString& filter_tag,
      blink::WebServiceWorkerRegistration* service_worker_registration,
      blink::WebNotificationGetCallbacks* callbacks) override;
  void close(blink::WebNotificationDelegate* delegate) override;
  void closePersistent(const blink::WebSecurityOrigin& origin,
                       int64_t persistent_notification_id) override;
  void notifyDelegateDestroyed(blink::WebNotificationDelegate* delegate)
      override;
  blink::WebNotificationPermission checkPermission(
      const blink::WebSecurityOrigin& origin) override;
  size_t maxActions() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNotificationManagerImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_NOTIFICATION_MANAGER_IMPL_H_
