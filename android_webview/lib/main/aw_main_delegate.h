// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_LIB_MAIN_AW_MAIN_DELEGATE_H_
#define ANDROID_WEBVIEW_LIB_MAIN_AW_MAIN_DELEGATE_H_

#include <memory>

#include "android_webview/browser/jni_dependency_factory.h"
#include "android_webview/common/aw_content_client.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/app/content_main_delegate.h"

namespace content {
class BrowserMainRunner;
}

namespace android_webview {

class AwContentBrowserClient;
class AwContentGpuClient;
class AwContentRendererClient;

// Android WebView implementation of ContentMainDelegate.
class AwMainDelegate : public content::ContentMainDelegate,
                       public JniDependencyFactory {
 public:
  AwMainDelegate();
  ~AwMainDelegate() override;

 private:
  // content::ContentMainDelegate implementation:
  bool BasicStartupComplete(int* exit_code) override;
  void PreSandboxStartup() override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  void ProcessExiting(const std::string& process_type) override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
  content::ContentGpuClient* CreateContentGpuClient() override;
  content::ContentRendererClient* CreateContentRendererClient() override;

  // JniDependencyFactory implementation.
  scoped_refptr<AwQuotaManagerBridge> CreateAwQuotaManagerBridge(
      AwBrowserContext* browser_context) override;
  content::WebContentsViewDelegate* CreateViewDelegate(
      content::WebContents* web_contents) override;
  AwWebPreferencesPopulater* CreateWebPreferencesPopulater() override;
  AwMessagePortService* CreateAwMessagePortService() override;
  AwLocaleManager* CreateAwLocaleManager() override;
#if defined(VIDEO_HOLE)
  content::ExternalVideoSurfaceContainer* CreateExternalVideoSurfaceContainer(
      content::WebContents* web_contents) override;
#endif

  std::unique_ptr<content::BrowserMainRunner> browser_runner_;
  AwContentClient content_client_;
  std::unique_ptr<AwContentBrowserClient> content_browser_client_;
  std::unique_ptr<AwContentGpuClient> content_gpu_client_;
  std::unique_ptr<AwContentRendererClient> content_renderer_client_;

  DISALLOW_COPY_AND_ASSIGN(AwMainDelegate);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_LIB_MAIN_AW_MAIN_DELEGATE_H_
