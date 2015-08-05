// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_notification_manager_impl.h"

#include "base/logging.h"

namespace html_viewer {

WebNotificationManagerImpl::WebNotificationManagerImpl() {}

WebNotificationManagerImpl::~WebNotificationManagerImpl() {}

void WebNotificationManagerImpl::show(const blink::WebSecurityOrigin&,
                                      const blink::WebNotificationData&,
                                      blink::WebNotificationDelegate*) {
  NOTIMPLEMENTED();
}

void WebNotificationManagerImpl::showPersistent(
    const blink::WebSecurityOrigin&,
    const blink::WebNotificationData&,
    blink::WebServiceWorkerRegistration*,
    blink::WebNotificationShowCallbacks*) {
  NOTIMPLEMENTED();
}

void WebNotificationManagerImpl::getNotifications(
    const blink::WebString& filterTag,
    blink::WebServiceWorkerRegistration*,
    blink::WebNotificationGetCallbacks*) {
  NOTIMPLEMENTED();
}

void WebNotificationManagerImpl::close(blink::WebNotificationDelegate*) {
  NOTIMPLEMENTED();
}

void WebNotificationManagerImpl::closePersistent(
    const blink::WebSecurityOrigin&,
    int64_t persistentNotificationId) {
  NOTIMPLEMENTED();
}

void WebNotificationManagerImpl::closePersistent(
    const blink::WebSecurityOrigin&,
    const blink::WebString& persistentNotificationId) {
  NOTIMPLEMENTED();
}

void WebNotificationManagerImpl::notifyDelegateDestroyed(
    blink::WebNotificationDelegate*) {
  NOTIMPLEMENTED();
}

blink::WebNotificationPermission WebNotificationManagerImpl::checkPermission(
    const blink::WebSecurityOrigin&) {
  NOTIMPLEMENTED();
  return blink::WebNotificationPermission();
}

size_t WebNotificationManagerImpl::maxActions() {
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace html_viewer
