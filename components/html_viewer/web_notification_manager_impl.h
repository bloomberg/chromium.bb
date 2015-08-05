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
  virtual ~WebNotificationManagerImpl();

  // blink::WebNotificationManager methods:
  virtual void show(const blink::WebSecurityOrigin&,
                    const blink::WebNotificationData&,
                    blink::WebNotificationDelegate*);
  virtual void showPersistent(const blink::WebSecurityOrigin&,
                              const blink::WebNotificationData&,
                              blink::WebServiceWorkerRegistration*,
                              blink::WebNotificationShowCallbacks*);
  virtual void getNotifications(const blink::WebString& filterTag,
                                blink::WebServiceWorkerRegistration*,
                                blink::WebNotificationGetCallbacks*);
  virtual void close(blink::WebNotificationDelegate*);
  virtual void closePersistent(const blink::WebSecurityOrigin&,
                               int64_t persistentNotificationId);
  virtual void closePersistent(
      const blink::WebSecurityOrigin&,
      const blink::WebString& persistentNotificationId);
  virtual void notifyDelegateDestroyed(blink::WebNotificationDelegate*);
  virtual blink::WebNotificationPermission checkPermission(
      const blink::WebSecurityOrigin&);
  virtual size_t maxActions();

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNotificationManagerImpl);
};

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_WEB_NOTIFICATION_MANAGER_IMPL_H_
