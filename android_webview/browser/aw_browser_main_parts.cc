// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_main_parts.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_devtools_delegate.h"
#include "android_webview/browser/aw_result_codes.h"
#include "base/android/build_info.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/result_codes.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

namespace android_webview {

bool UseCompositorDirectDraw() {
  return base::android::BuildInfo::GetInstance()->sdk_int() >= 16;
}

AwBrowserMainParts::AwBrowserMainParts(AwBrowserContext* browser_context)
    : browser_context_(browser_context) {
}

AwBrowserMainParts::~AwBrowserMainParts() {
}

void AwBrowserMainParts::PreEarlyInitialization() {
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());
  content::Compositor::InitializeWithFlags(
      content::Compositor::DIRECT_CONTEXT_ON_DRAW_THREAD);

  // Android WebView does not use default MessageLoop. It has its own
  // Android specific MessageLoop. Also see MainMessageLoopRun.
  DCHECK(!main_message_loop_.get());
  main_message_loop_.reset(new MessageLoop(MessageLoop::TYPE_UI));
  MessageLoopForUI::current()->Start();
}

int AwBrowserMainParts::PreCreateThreads() {
  browser_context_->InitializeBeforeThreadCreation();

  ui::ResourceBundle::InitSharedInstanceLocaleOnly(
      content::GetContentClient()->browser()->GetApplicationLocale(), NULL);

  base::FilePath pak_path;
  PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_path);

  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_path.AppendASCII("webviewchromium.pak"),
      ui::SCALE_FACTOR_NONE);

  return content::RESULT_CODE_NORMAL_EXIT;
}

void AwBrowserMainParts::PreMainMessageLoopRun() {
  browser_context_->PreMainMessageLoopRun();
  devtools_delegate_ = new AwDevToolsDelegate(browser_context_);
}

bool AwBrowserMainParts::MainMessageLoopRun(int* result_code) {
  // Android WebView does not use default MessageLoop. It has its own
  // Android specific MessageLoop.
  return true;
}

void AwBrowserMainParts::PostMainMessageLoopRun() {
  if (devtools_delegate_)
    devtools_delegate_->Stop();
}

}  // namespace android_webview
