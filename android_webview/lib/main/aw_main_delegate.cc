// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/main/aw_main_delegate.h"

#include "android_webview/common/url_constants.h"
#include "android_webview/lib/aw_browser_dependency_factory_impl.h"
#include "android_webview/lib/aw_content_browser_client.h"
#include "android_webview/renderer/aw_render_view_ext.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_client.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityPolicy.h"

namespace android_webview {

namespace {

// TODO(joth): Remove chrome/ dependency; move into android_webview/renderer
class AwContentRendererClient : public chrome::ChromeContentRendererClient {
 public:
  virtual void RenderViewCreated(content::RenderView* render_view) OVERRIDE {
    chrome::ChromeContentRendererClient::RenderViewCreated(render_view);
    AwRenderViewExt::RenderViewCreated(render_view);
  }

  virtual void RenderThreadStarted() OVERRIDE {
    chrome::ChromeContentRendererClient::RenderThreadStarted();
    WebKit::WebString content_scheme(
        ASCIIToUTF16(android_webview::kContentScheme));
    WebKit::WebSecurityPolicy::registerURLSchemeAsLocal(content_scheme);
  }
};

}

base::LazyInstance<AwContentBrowserClient>
    g_webview_content_browser_client = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<AwContentRendererClient>
    g_webview_content_renderer_client = LAZY_INSTANCE_INITIALIZER;

AwMainDelegate::AwMainDelegate() {
}

AwMainDelegate::~AwMainDelegate() {
}

bool AwMainDelegate::BasicStartupComplete(int* exit_code) {
  content::SetContentClient(&chrome_content_client_);

  return false;
}

void AwMainDelegate::PreSandboxStartup() {
  chrome::RegisterPathProvider();

  // TODO(torne): When we have a separate renderer process, we need to handle
  // being passed open FDs for the resource paks here.
}

void AwMainDelegate::SandboxInitialized(const std::string& process_type) {
  // TODO(torne): Adjust linux OOM score here.
}

int AwMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    AwBrowserDependencyFactoryImpl::InstallInstance();

    browser_runner_.reset(content::BrowserMainRunner::Create());
    int exit_code = browser_runner_->Initialize(main_function_params);
    DCHECK(exit_code < 0);

    // Return 0 so that we do NOT trigger the default behavior. On Android, the
    // UI message loop is managed by the Java application.
    return 0;
  }

  return -1;
}

void AwMainDelegate::ProcessExiting(const std::string& process_type) {
  // TODO(torne): Clean up resources when we handle them.

  logging::CloseLogFile();
}

content::ContentBrowserClient*
    AwMainDelegate::CreateContentBrowserClient() {
  return &g_webview_content_browser_client.Get();
}

content::ContentRendererClient*
    AwMainDelegate::CreateContentRendererClient() {
  return &g_webview_content_renderer_client.Get();
}

}  // namespace android_webview
