// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/layout_test/layout_test_permission_manager.h"

#include "base/bind.h"
#include "base/callback.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_type.h"
#include "content/shell/browser/layout_test/layout_test_content_browser_client.h"
#include "content/shell/browser/layout_test/layout_test_notification_manager.h"
#include "url/gurl.h"

namespace content {

namespace {

void RequestDesktopNotificationPermissionOnIO(
    const GURL& origin,
    const base::Callback<void(PermissionStatus)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  LayoutTestNotificationManager* manager =
      LayoutTestContentBrowserClient::Get()->GetLayoutTestNotificationManager();
  PermissionStatus result = manager ? manager->RequestPermission(origin)
                                    : PERMISSION_STATUS_GRANTED;

  // The callback came from the UI thread, we need to run it from there again.
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(callback, result));
}

}  // anonymous namespace

LayoutTestPermissionManager::LayoutTestPermissionManager()
    : ShellPermissionManager() {
}

LayoutTestPermissionManager::~LayoutTestPermissionManager() {
}

void LayoutTestPermissionManager::RequestPermission(
    PermissionType permission,
    WebContents* web_contents,
    int request_id,
    const GURL& requesting_origin,
    bool user_gesture,
    const base::Callback<void(PermissionStatus)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (permission == PermissionType::NOTIFICATIONS) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&RequestDesktopNotificationPermissionOnIO,
                   requesting_origin,
                   callback));
    return;
  }

  ShellPermissionManager::RequestPermission(permission,
                                            web_contents,
                                            request_id,
                                            requesting_origin,
                                            user_gesture,
                                            callback);
}

}  // namespace content
