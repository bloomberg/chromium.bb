// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/test_runner/notification_presenter.h"

#include "base/logging.h"
#include "content/common/desktop_notification_messages.h"
#include "content/shell/renderer/test_runner/web_test_delegate.h"
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

bool NotificationPresenter::SimulateClick(const std::string& title) {
  ActiveNotificationMap::iterator iter = active_notifications_.find(title);
  if (iter == active_notifications_.end())
    return false;

  const WebNotification& notification = iter->second;

  WebNotification event_target(notification);
  event_target.dispatchClickEvent();

  return true;
}

void NotificationPresenter::Reset() {
  while (!active_notifications_.empty()) {
    const WebNotification& notification = active_notifications_.begin()->second;
    cancel(notification);
  }

  replacements_.clear();
}

bool NotificationPresenter::show(const WebNotification& notification) {
  if (!notification.replaceId().isEmpty()) {
    std::string replaceId(notification.replaceId().utf8());
    if (replacements_.find(replaceId) != replacements_.end()) {
      delegate_->PrintMessage(std::string("REPLACING NOTIFICATION ") +
                              replacements_.find(replaceId)->second + "\n");
    }
    replacements_[replaceId] = notification.title().utf8();
  }

  delegate_->PrintMessage("DESKTOP NOTIFICATION SHOWN: ");
  if (!notification.title().isEmpty())
    delegate_->PrintMessage(notification.title().utf8().data());

  if (notification.direction() == WebTextDirectionRightToLeft)
    delegate_->PrintMessage(", RTL");

  // TODO(beverloo): WebNotification should expose the "lang" attribute's value.

  if (!notification.body().isEmpty()) {
    delegate_->PrintMessage(std::string(", body: ") +
                            notification.body().utf8().data());
  }

  if (!notification.replaceId().isEmpty()) {
    delegate_->PrintMessage(std::string(", tag: ") +
                            notification.replaceId().utf8().data());
  }

  if (!notification.iconURL().isEmpty()) {
    delegate_->PrintMessage(std::string(", icon: ") +
                            notification.iconURL().spec().data());
  }

  delegate_->PrintMessage("\n");

  std::string title = notification.title().utf8();
  active_notifications_[title] = notification;

  Platform::current()->callOnMainThread(DeferredDisplayDispatch,
                                        new WebNotification(notification));

  return true;
}

void NotificationPresenter::cancel(const WebNotification& notification) {
  std::string title = notification.title().utf8();

  delegate_->PrintMessage(std::string("DESKTOP NOTIFICATION CLOSED: ") + title +
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
  return delegate_->CheckWebNotificationPermission(
      GURL(security_origin.toString()));
}

}  // namespace content
