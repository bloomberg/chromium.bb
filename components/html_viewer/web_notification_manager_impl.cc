// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/web_notification_manager_impl.h"

#include "base/logging.h"

namespace html_viewer {

WebNotificationManagerImpl::WebNotificationManagerImpl() {}

WebNotificationManagerImpl::~WebNotificationManagerImpl() {}

void WebNotificationManagerImpl::show(
    const blink::WebSecurityOrigin& origin,
    const blink::WebNotificationData& notification_data,
    blink::WebNotificationDelegate* delegate) {
  NOTIMPLEMENTED();
}

void WebNotificationManagerImpl::showPersistent(
    const blink::WebSecurityOrigin& origin,
    const blink::WebNotificationData& notification_data,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebNotificationShowCallbacks* callbacks) {
  NOTIMPLEMENTED();
}

void WebNotificationManagerImpl::getNotifications(
    const blink::WebString& filter_tag,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebNotificationGetCallbacks* callbacks) {
  NOTIMPLEMENTED();
}

void WebNotificationManagerImpl::close(
    blink::WebNotificationDelegate* delegate) {
  NOTIMPLEMENTED();
}

void WebNotificationManagerImpl::closePersistent(
    const blink::WebSecurityOrigin& origin,
    int64_t persistent_notification_id) {
  NOTIMPLEMENTED();
}

void WebNotificationManagerImpl::notifyDelegateDestroyed(
    blink::WebNotificationDelegate* delegate) {
  NOTIMPLEMENTED();
}

blink::WebNotificationPermission WebNotificationManagerImpl::checkPermission(
    const blink::WebSecurityOrigin& origin) {
  NOTIMPLEMENTED();
  return blink::WebNotificationPermissionDenied;
}

size_t WebNotificationManagerImpl::maxActions() {
  NOTIMPLEMENTED();
  return 0;
}

}  // namespace html_viewer
