// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_main_parts.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_result_codes.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/result_codes.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/resource/resource_bundle.h"

namespace android_webview {

AwBrowserMainParts::AwBrowserMainParts(AwBrowserContext* browser_context)
    : browser_context_(browser_context) {
}

AwBrowserMainParts::~AwBrowserMainParts() {
}

void AwBrowserMainParts::PreEarlyInitialization() {
  net::NetworkChangeNotifier::SetFactory(
      new net::NetworkChangeNotifierFactoryAndroid());

  // Android WebView does not use default MessageLoop. It has its own
  // Android specific MessageLoop. ALso see MainMessageLoopRun.
  DCHECK(!main_message_loop_.get());
  main_message_loop_.reset(new MessageLoop(MessageLoop::TYPE_UI));
  MessageLoopForUI::current()->Start();
}

int AwBrowserMainParts::PreCreateThreads() {
  browser_context_->InitializeBeforeThreadCreation();
  const std::string loaded_locale =
      ui::ResourceBundle::InitSharedInstanceWithLocale(
          content::GetContentClient()->browser()->GetApplicationLocale(), NULL);
  if (loaded_locale.empty())
    return RESULT_CODE_MISSING_DATA;
  return content::RESULT_CODE_NORMAL_EXIT;
}

bool AwBrowserMainParts::MainMessageLoopRun(int* result_code) {
  // Android WebView does not use default MessageLoop. It has its own
  // Android specific MessageLoop.
  return true;
}

}  // namespace android_webview
