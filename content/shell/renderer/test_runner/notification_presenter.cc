// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/notification_presenter.h"

#include "content/shell/renderer/test_runner/WebTestDelegate.h"
#include "third_party/WebKit/public/platform/Platform.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebNotificationPermissionCallback.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "url/gurl.h"

using blink::Platform;
using blink::WebNotification;
using blink::WebNotificationPermissionCallback;
using blink::WebNotificationPresenter;
using blink::WebSecurityOrigin;
using blink::WebTextDirectionRightToLeft;

namespace content {

namespace {

void DeferredDisplayDispatch(void* context) {
  WebNotification* notification = static_cast<WebNotification*>(context);
  notification->dispatchDisplayEvent();

  delete notification;
}

}  // namespace

NotificationPresenter::NotificationPresenter() : delegate_(0) {}

NotificationPresenter::~NotificationPresenter() {}

void NotificationPresenter::GrantPermission(const std::string& origin) {
  allowed_origins_.insert(origin);
}

bool NotificationPresenter::SimulateClick(const std::string& title) {
  ActiveNotificationMap::iterator iter = active_notifications_.find(title);
  if (iter == active_notifications_.end())
    return false;

  const WebNotification& notification = iter->second;

  WebNotification event_target(notification);
  event_target.dispatchClickEvent();

  return true;
}

void NotificationPresenter::CancelAllActiveNotifications() {
  while (!active_notifications_.empty()) {
    const WebNotification& notification = active_notifications_.begin()->second;
    cancel(notification);
  }
}

void NotificationPresenter::Reset() {
  // TODO(peter): Ensure that |active_notifications_| is empty as well.
  allowed_origins_.clear();
}

bool NotificationPresenter::show(const WebNotification& notification) {
  if (!notification.replaceId().isEmpty()) {
    std::string replaceId(notification.replaceId().utf8());
    if (replacements_.find(replaceId) != replacements_.end()) {
      delegate_->printMessage(std::string("REPLACING NOTIFICATION ") +
                              replacements_.find(replaceId)->second + "\n");
    }
    replacements_[replaceId] = notification.title().utf8();
  }

  delegate_->printMessage("DESKTOP NOTIFICATION:");
  delegate_->printMessage(
      notification.direction() == WebTextDirectionRightToLeft ? "(RTL)" : "");
  delegate_->printMessage(" icon ");
  delegate_->printMessage(notification.iconURL().isEmpty()
                              ? ""
                              : notification.iconURL().spec().data());
  delegate_->printMessage(", title ");
  delegate_->printMessage(
      notification.title().isEmpty() ? "" : notification.title().utf8().data());
  delegate_->printMessage(", text ");
  delegate_->printMessage(
      notification.body().isEmpty() ? "" : notification.body().utf8().data());
  delegate_->printMessage("\n");

  std::string title = notification.title().utf8();
  active_notifications_[title] = notification;

  Platform::current()->callOnMainThread(DeferredDisplayDispatch,
                                        new WebNotification(notification));

  return true;
}

void NotificationPresenter::cancel(const WebNotification& notification) {
  std::string title = notification.title().utf8();

  delegate_->printMessage(std::string("DESKTOP NOTIFICATION CLOSED: ") + title +
                          "\n");

  WebNotification event_target(notification);
  event_target.dispatchCloseEvent(false);

  active_notifications_.erase(title);
}

void NotificationPresenter::objectDestroyed(
    const WebNotification& notification) {
  std::string title = notification.title().utf8();
  active_notifications_.erase(title);
}

WebNotificationPresenter::Permission NotificationPresenter::checkPermission(
    const WebSecurityOrigin& security_origin) {
  const std::string origin = security_origin.toString().utf8();
  if (allowed_origins_.find(origin) != allowed_origins_.end())
    return WebNotificationPresenter::PermissionAllowed;

  return WebNotificationPresenter::PermissionDenied;
}

void NotificationPresenter::requestPermission(
    const WebSecurityOrigin& security_origin,
    WebNotificationPermissionCallback* callback) {
  std::string origin = security_origin.toString().utf8();
  delegate_->printMessage("DESKTOP NOTIFICATION PERMISSION REQUESTED: " +
                          origin + "\n");
  callback->permissionRequestComplete();
}

}  // namespace content
