// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_CONTENT_BROWSER_CLIENT_H_

#include "content/public/browser/permission_type.h"
#include "content/shell/browser/shell_content_browser_client.h"

namespace content {

class LayoutTestBrowserContext;
class LayoutTestNotificationManager;

class LayoutTestContentBrowserClient : public ShellContentBrowserClient {
 public:
  // Gets the current instance.
  static LayoutTestContentBrowserClient* Get();

  LayoutTestContentBrowserClient();
  ~LayoutTestContentBrowserClient() override;

  LayoutTestBrowserContext* GetLayoutTestBrowserContext();

  // Implements the PlatformNotificationService interface.
  LayoutTestNotificationManager* GetLayoutTestNotificationManager();

  // ContentBrowserClient overrides.
  void RenderProcessWillLaunch(RenderProcessHost* host) override;
  void RequestPermission(
      PermissionType permission,
      WebContents* web_contents,
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      const base::Callback<void(PermissionStatus)>& callback) override;
  PlatformNotificationService* GetPlatformNotificationService() override;
  void GetAdditionalNavigatorConnectServices(
      const scoped_refptr<NavigatorConnectContext>& context) override;

 private:
  scoped_ptr<LayoutTestNotificationManager> layout_test_notification_manager_;
};

}  // content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_CONTENT_BROWSER_CLIENT_H_
