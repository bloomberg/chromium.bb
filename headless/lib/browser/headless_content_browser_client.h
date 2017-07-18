// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_CONTENT_BROWSER_CLIENT_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_CONTENT_BROWSER_CLIENT_H_

#include "content/public/browser/content_browser_client.h"
#include "headless/lib/browser/headless_resource_dispatcher_host_delegate.h"

namespace headless {

class HeadlessBrowserImpl;

class HeadlessContentBrowserClient : public content::ContentBrowserClient {
 public:
  explicit HeadlessContentBrowserClient(HeadlessBrowserImpl* browser);
  ~HeadlessContentBrowserClient() override;

  // content::ContentBrowserClient implementation:
  content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams&) override;
  void OverrideWebkitPrefs(content::RenderViewHost* render_view_host,
                           content::WebPreferences* prefs) override;
  content::DevToolsManagerDelegate* GetDevToolsManagerDelegate() override;
  std::unique_ptr<base::Value> GetServiceManifestOverlay(
      base::StringPiece name) override;
  content::QuotaPermissionContext* CreateQuotaPermissionContext() override;
  void GetQuotaSettings(
      content::BrowserContext* context,
      content::StoragePartition* partition,
      storage::OptionalQuotaSettingsCallback callback) override;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::FileDescriptorInfo* mappings) override;
#endif
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;

  void AllowCertificateError(
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      content::ResourceType resource_type,
      bool overridable,
      bool strict_enforcement,
      bool expired_previous_decision,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback) override;

  void ResourceDispatcherHostCreated() override;

  net::NetLog* GetNetLog() override;

 private:
  std::unique_ptr<base::Value> GetBrowserServiceManifestOverlay();
  std::unique_ptr<base::Value> GetRendererServiceManifestOverlay();

  HeadlessBrowserImpl* browser_;  // Not owned.

  std::unique_ptr<HeadlessResourceDispatcherHostDelegate>
      resource_dispatcher_host_delegate_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessContentBrowserClient);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_CONTENT_BROWSER_CLIENT_H_
