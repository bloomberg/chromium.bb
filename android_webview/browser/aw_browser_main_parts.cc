// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_main_parts.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_result_codes.h"
#include "base/android/build_info.h"
#include "base/android/memory_pressure_listener_android.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_utils.h"
#include "gpu/command_buffer/service/mailbox_synchronizer.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"

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
  // Android specific MessageLoop. Also see MainMessageLoopRun.
  DCHECK(!main_message_loop_.get());
  main_message_loop_.reset(new base::MessageLoopForUI);
  base::MessageLoopForUI::current()->Start();
}

int AwBrowserMainParts::PreCreateThreads() {
  ui::ResourceBundle::InitSharedInstanceLocaleOnly(
      l10n_util::GetDefaultLocale(), NULL);

  base::FilePath pak_path;
  PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_path);

  ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
      pak_path.AppendASCII("webviewchromium.pak"),
      ui::SCALE_FACTOR_NONE);

  base::android::MemoryPressureListenerAndroid::RegisterSystemCallback(
      base::android::AttachCurrentThread());

  return content::RESULT_CODE_NORMAL_EXIT;
}

void AwBrowserMainParts::PreMainMessageLoopRun() {
  // TODO(boliu): Can't support accelerated 2d canvas and WebGL with ubercomp
  // yet: crbug.com/352424.
  if (!gpu::gles2::MailboxSynchronizer::Initialize()) {
    CommandLine* cl = CommandLine::ForCurrentProcess();
    cl->AppendSwitch(switches::kDisableAccelerated2dCanvas);
    cl->AppendSwitch(switches::kDisableExperimentalWebGL);
  }

  browser_context_->PreMainMessageLoopRun();
  // This is needed for WebView Classic backwards compatibility
  // See crbug.com/298495
  content::SetMaxURLChars(20 * 1024 * 1024);
}

bool AwBrowserMainParts::MainMessageLoopRun(int* result_code) {
  // Android WebView does not use default MessageLoop. It has its own
  // Android specific MessageLoop.
  return true;
}

}  // namespace android_webview
