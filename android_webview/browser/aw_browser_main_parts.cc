// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_main_parts.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_result_codes.h"
#include "base/android/build_info.h"
#include "base/file_path.h"
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

  // Although we don't load a locale pak for Android WebView, we still
  // need to initialise the resource bundle with a locale.
  ui::ResourceBundle::InitSharedInstanceLocaleOnly(
      content::GetContentClient()->browser()->GetApplicationLocale(), NULL);

  // TODO(benm): It would be nice to be able to take string resources from
  // the Android framework for WebView, and rely on resource paks only
  // for non-string assets. e.g. graphics (and might be nice in the
  // long term to move them into the Android framwork too). Toward
  // that, seed the ResourceBundle instance with a non-string, locale
  // independant pak. Until we no longer rely on paks for strings,
  // load an extra data pack separately that has the strings in it.
  FilePath pak_path;
  PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_path);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_path.AppendASCII("webviewchromium_strings.pak"),
      ui::SCALE_FACTOR_NONE);
  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_path.AppendASCII("webviewchromium.pak"),
      ui::SCALE_FACTOR_NONE);

  return content::RESULT_CODE_NORMAL_EXIT;
}

bool AwBrowserMainParts::MainMessageLoopRun(int* result_code) {
  // Android WebView does not use default MessageLoop. It has its own
  // Android specific MessageLoop.
  return true;
}

}  // namespace android_webview
