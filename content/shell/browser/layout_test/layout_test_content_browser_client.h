// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_CONTENT_BROWSER_CLIENT_H_

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
  void ExposeInterfacesToRenderer(
      service_manager::InterfaceRegistry* registry,
      RenderProcessHost* render_process_host) override;
  void OverrideWebkitPrefs(RenderViewHost* render_view_host,
                           WebPreferences* prefs) override;
  void ResourceDispatcherHostCreated() override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  BrowserMainParts* CreateBrowserMainParts(
      const MainFunctionParams& parameters) override;
  void GetQuotaSettings(
      content::BrowserContext* context,
      content::StoragePartition* partition,
      const storage::OptionalQuotaSettingsCallback& callback) override;

  PlatformNotificationService* GetPlatformNotificationService() override;

 private:
  std::unique_ptr<LayoutTestNotificationManager>
      layout_test_notification_manager_;
};

}  // content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_CONTENT_BROWSER_CLIENT_H_
