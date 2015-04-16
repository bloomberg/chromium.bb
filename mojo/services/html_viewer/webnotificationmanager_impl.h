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
  virtual ~WebNotificationManagerImpl();

  // blink::WebNotificationManager methods:
  virtual void show(const blink::WebSerializedOrigin&,
                    const blink::WebNotificationData&,
                    blink::WebNotificationDelegate*);
  virtual void showPersistent(const blink::WebSerializedOrigin&,
                              const blink::WebNotificationData&,
                              blink::WebServiceWorkerRegistration*,
                              blink::WebNotificationShowCallbacks*);
  virtual void getNotifications(const blink::WebString& filterTag,
                                blink::WebServiceWorkerRegistration*,
                                blink::WebNotificationGetCallbacks*);
  virtual void close(blink::WebNotificationDelegate*);
  virtual void closePersistent(const blink::WebSerializedOrigin&,
                               int64_t persistentNotificationId);
  virtual void closePersistent(
      const blink::WebSerializedOrigin&,
      const blink::WebString& persistentNotificationId);
  virtual void notifyDelegateDestroyed(blink::WebNotificationDelegate*);
  virtual blink::WebNotificationPermission checkPermission(
      const blink::WebSerializedOrigin&);

 private:
  DISALLOW_COPY_AND_ASSIGN(WebNotificationManagerImpl);
};

}  // namespace html_viewer

#endif  // MOJO_SERVICES_HTML_VIEWER_WEBNOTIFICATIONMANAGER_IMPL_H_
