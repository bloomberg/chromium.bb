// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_notification_manager.h"

#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/common/show_desktop_notification_params.h"

namespace content {

LayoutTestNotificationManager::LayoutTestNotificationManager()
    : weak_factory_(this) {}

LayoutTestNotificationManager::~LayoutTestNotificationManager() {}

blink::WebNotificationPermission
LayoutTestNotificationManager::CheckPermission(const GURL& origin) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  NotificationPermissionMap::iterator iter =
      permission_map_.find(origin);
  if (iter == permission_map_.end())
    return blink::WebNotificationPermissionDefault;

  return iter->second;
}

void LayoutTestNotificationManager::RequestPermission(
    const GURL& origin,
    const base::Callback<void(bool)>& callback) {
  bool allowed =
      CheckPermission(origin) == blink::WebNotificationPermissionAllowed;
  callback.Run(allowed);
}

void LayoutTestNotificationManager::SetPermission(
    const GURL& origin,
    blink::WebNotificationPermission permission) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  permission_map_[origin] = permission;
}

void LayoutTestNotificationManager::ClearPermissions() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  permission_map_.clear();
}

void LayoutTestNotificationManager::Show(
    const ShowDesktopNotificationHostMsgParams& params,
    scoped_ptr<DesktopNotificationDelegate> delegate,
    base::Closure* cancel_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string title = base::UTF16ToUTF8(params.title);

  DCHECK(cancel_callback);
  *cancel_callback = base::Bind(&LayoutTestNotificationManager::Close,
                                weak_factory_.GetWeakPtr(),
                                title);

  if (params.replace_id.length()) {
    std::string replace_id = base::UTF16ToUTF8(params.replace_id);
    auto replace_iter = replacements_.find(replace_id);
    if (replace_iter != replacements_.end()) {
      const std::string& previous_title = replace_iter->second;
      auto notification_iter = notifications_.find(previous_title);
      if (notification_iter != notifications_.end()) {
        DesktopNotificationDelegate* previous_delegate =
            notification_iter->second;
        previous_delegate->NotificationClosed(false);

        notifications_.erase(notification_iter);
        delete previous_delegate;
      }
    }

    replacements_[replace_id] = title;
  }

  notifications_[title] = delegate.release();
  notifications_[title]->NotificationDisplayed();
}

void LayoutTestNotificationManager::SimulateClick(const std::string& title) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auto iterator = notifications_.find(title);
  if (iterator == notifications_.end())
    return;

  iterator->second->NotificationClick();
}

void LayoutTestNotificationManager::Close(const std::string& title) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auto iterator = notifications_.find(title);
  if (iterator == notifications_.end())
    return;

  iterator->second->NotificationClosed(false);
}

}  // namespace content
