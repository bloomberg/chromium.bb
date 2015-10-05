// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_main_parts.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_dev_tools_discovery_provider.h"
#include "android_webview/browser/aw_media_client_android.h"
#include "android_webview/browser/aw_result_codes.h"
#include "android_webview/browser/deferred_gpu_command_service.h"
#include "android_webview/common/aw_resource.h"
#include "android_webview/common/aw_switches.h"
#include "base/android/apk_assets.h"
#include "base/android/build_info.h"
#include "base/android/locale_utils.h"
#include "base/android/memory_pressure_listener_android.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_utils.h"
#include "media/base/android/media_client_android.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/gl_surface.h"

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
  ui::SetLocalePaksStoredInApk(true);
  std::string locale = ui::ResourceBundle::InitSharedInstanceWithLocale(
      base::android::GetDefaultLocale(),
      NULL,
      ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  if (locale.empty()) {
    LOG(WARNING) << "Failed to load locale .pak from the apk. "
        "Bringing up WebView without any locale";
  }

  // Try to directly mmap the webviewchromium.pak from the apk. Fall back to
  // load from file, using PATH_SERVICE, otherwise.
  base::FilePath pak_file_path;
  PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_file_path);
  pak_file_path = pak_file_path.AppendASCII("webviewchromium.pak");
  ui::LoadMainAndroidPackFile("assets/webviewchromium.pak", pak_file_path);

  base::android::MemoryPressureListenerAndroid::RegisterSystemCallback(
      base::android::AttachCurrentThread());
  DeferredGpuCommandService::SetInstance();

  return content::RESULT_CODE_NORMAL_EXIT;
}

void AwBrowserMainParts::PreMainMessageLoopRun() {
  browser_context_->PreMainMessageLoopRun();

  AwDevToolsDiscoveryProvider::Install();

  media::SetMediaClientAndroid(
      new AwMediaClientAndroid(AwResource::GetConfigKeySystemUuidMapping()));

  content::RenderFrameHost::AllowInjectingJavaScriptForAndroidWebView();

  // This is needed for WebView Classic backwards compatibility
  // See crbug.com/298495. Also see crbug.com/525697 on why it is currently
  // for single process mode only.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    content::SetMaxURLChars(20 * 1024 * 1024);
  }
}

bool AwBrowserMainParts::MainMessageLoopRun(int* result_code) {
  // Android WebView does not use default MessageLoop. It has its own
  // Android specific MessageLoop.
  return true;
}

}  // namespace android_webview
