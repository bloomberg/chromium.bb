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
  void SetPopupBlockingEnabled(bool block_popups_);

  // Implements the PlatformNotificationService interface.
  LayoutTestNotificationManager* GetLayoutTestNotificationManager();

  // ContentBrowserClient overrides.
  void RenderProcessWillLaunch(RenderProcessHost* host) override;
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
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
      storage::OptionalQuotaSettingsCallback callback) override;
  bool DoesSiteRequireDedicatedProcess(BrowserContext* browser_context,
                                       const GURL& effective_site_url) override;

  PlatformNotificationService* GetPlatformNotificationService() override;

  bool CanCreateWindow(content::RenderFrameHost* opener,
                       const GURL& opener_url,
                       const GURL& opener_top_level_frame_url,
                       const GURL& source_origin,
                       content::mojom::WindowContainerType container_type,
                       const GURL& target_url,
                       const content::Referrer& referrer,
                       const std::string& frame_name,
                       WindowOpenDisposition disposition,
                       const blink::mojom::WindowFeatures& features,
                       bool user_gesture,
                       bool opener_suppressed,
                       bool* no_javascript_access) override;

  // ShellContentBrowserClient overrides.
  void ExposeInterfacesToFrame(
      service_manager::BinderRegistryWithArgs<content::RenderFrameHost*>*
          registry) override;

 private:
  std::unique_ptr<LayoutTestNotificationManager>
      layout_test_notification_manager_;
  bool block_popups_ = false;
};

}  // content

#endif  // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_CONTENT_BROWSER_CLIENT_H_
