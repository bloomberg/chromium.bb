// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_WEBNOTIFICATIONMANAGER_IMPL_H_
#define MOJO_SERVICES_HTML_VIEWER_WEBNOTIFICATIONMANAGER_IMPL_H_

#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationManager.h"

namespace html_viewer {

// TODO(erg): This class is currently a stub; blink expects this object to
// exist, and several websites will trigger notifications these days.
class WebNotificationManagerImpl : public blink::WebNotificationManager {
 public:
  WebNotificationManagerImpl();
  ~WebNotificationManagerImpl() override;

  // blink::WebNotificationManager methods:
  void show(const blink::WebSerializedOrigin&,
            const blink::WebNotificationData&,
            blink::WebNotificationDelegate*) override;
  void showPersistent(const blink::WebSerializedOrigin&,
                      const blink::WebNotificationData&,
                      blink::WebServiceWorkerRegistration*,
                      blink::WebNotificationShowCallbacks*) override;
  void getNotifications(const blink::WebString& filterTag,
                        blink::WebServiceWorkerRegistration*,
                        blink::WebNotificationGetCallbacks*) override;
  void close(blink::WebNotificationDelegate*) override;
  void closePersistent(const blink::WebSerializedOrigin&,
                       int64_t persistentNotificationId) override;
  void closePersistent(const blink::WebSerializedOrigin&,
                       const blink::WebString& persistentNotificationId)
      override;
  void notifyDelegateDestroyed(blink::WebNotificationDelegate*) override;
  blink::WebNotificationPermission checkPermission(
      const blink::WebSerializedOrigin&) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNotificationManagerImpl);
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBNOTIFICATIONMANAGER_IMPL_H_
