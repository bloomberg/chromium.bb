// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_notification_manager.h"

#include "base/guid.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/desktop_notification_delegate.h"
#include "content/public/browser/notification_event_dispatcher.h"
#include "content/public/browser/permission_type.h"
#include "content/public/common/persistent_notification_status.h"
#include "content/public/common/platform_notification_data.h"
#include "content/shell/browser/layout_test/layout_test_browser_context.h"
#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"
#include "content/shell/browser/layout_test/layout_test_permission_manager.h"

namespace content {

LayoutTestNotificationManager::LayoutTestNotificationManager() {}
LayoutTestNotificationManager::~LayoutTestNotificationManager() {}

bool LayoutTestNotificationManager::GetDisplayedNotifications(
    BrowserContext* browser_context,
    std::set<std::string>* displayed_notifications) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(displayed_notifications);

  // Notifications will never outlive the lifetime of running layout tests.
  return false;
}

blink::mojom::PermissionStatus
LayoutTestNotificationManager::CheckPermission(const GURL& origin) {
  return LayoutTestContentBrowserClient::Get()
      ->GetLayoutTestBrowserContext()
      ->GetLayoutTestPermissionManager()
      ->GetPermissionStatus(PermissionType::NOTIFICATIONS,
                            origin,
                            origin);
}

}  // namespace content
