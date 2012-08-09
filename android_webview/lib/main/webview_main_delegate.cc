// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/main/webview_main_delegate.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/common/content_client.h"

namespace android_webview {

base::LazyInstance<chrome::ChromeContentBrowserClient>
    g_webview_content_browser_client = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<chrome::ChromeContentRendererClient>
    g_webview_content_renderer_client = LAZY_INSTANCE_INITIALIZER;

WebViewMainDelegate::WebViewMainDelegate() {
}

WebViewMainDelegate::~WebViewMainDelegate() {
}

bool WebViewMainDelegate::BasicStartupComplete(int* exit_code) {
  content::SetContentClient(&chrome_content_client_);

  return false;
}

void WebViewMainDelegate::PreSandboxStartup() {
  chrome::RegisterPathProvider();

  // TODO(torne): When we have a separate renderer process, we need to handle
  // being passed open FDs for the resource paks here.
}

void WebViewMainDelegate::SandboxInitialized(const std::string& process_type) {
  // TODO(torne): Adjust linux OOM score here.
}

int WebViewMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    browser_runner_.reset(content::BrowserMainRunner::Create());
    int exit_code = browser_runner_->Initialize(main_function_params);
    DCHECK(exit_code < 0);

    // Return 0 so that we do NOT trigger the default behavior. On Android, the
    // UI message loop is managed by the Java application.
    return 0;
  }

  return -1;
}

void WebViewMainDelegate::ProcessExiting(const std::string& process_type) {
  // TODO(torne): Clean up resources when we handle them.

  logging::CloseLogFile();
}

content::ContentBrowserClient*
    WebViewMainDelegate::CreateContentBrowserClient() {
  return &g_webview_content_browser_client.Get();
}

content::ContentRendererClient*
    WebViewMainDelegate::CreateContentRendererClient() {
  return &g_webview_content_renderer_client.Get();
}

}  // namespace android_webview
