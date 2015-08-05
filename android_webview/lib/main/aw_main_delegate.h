// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_LIB_MAIN_AW_MAIN_DELEGATE_H_
#define ANDROID_WEBVIEW_LIB_MAIN_AW_MAIN_DELEGATE_H_

#include "android_webview/browser/jni_dependency_factory.h"
#include "android_webview/common/aw_content_client.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/app/content_main_delegate.h"

namespace content {
class BrowserMainRunner;
class ExternalVideoSurfaceContainel;
}

namespace android_webview {

class AwContentBrowserClient;
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
  void SandboxInitialized(const std::string& process_type) override;
  int RunProcess(
      const std::string& process_type,
      const content::MainFunctionParams& main_function_params) override;
  void ProcessExiting(const std::string& process_type) override;
  content::ContentBrowserClient* CreateContentBrowserClient() override;
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

  scoped_ptr<content::BrowserMainRunner> browser_runner_;
  AwContentClient content_client_;
  scoped_ptr<AwContentBrowserClient> content_browser_client_;
  scoped_ptr<AwContentRendererClient> content_renderer_client_;

  DISALLOW_COPY_AND_ASSIGN(AwMainDelegate);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_LIB_MAIN_AW_MAIN_DELEGATE_H_
