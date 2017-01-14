// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_browser_main_parts.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_terminator.h"
#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/aw_result_codes.h"
#include "android_webview/browser/deferred_gpu_command_service.h"
#include "android_webview/browser/net/aw_network_change_notifier_factory.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/aw_paths.h"
#include "android_webview/common/aw_resource.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/crash_reporter/aw_microdump_crash_reporter.h"
#include "base/android/apk_assets.h"
#include "base/android/build_info.h"
#include "base/android/locale_utils.h"
#include "base/android/memory_pressure_listener_android.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "components/crash/content/browser/crash_dump_manager_android.h"
#include "components/crash/content/browser/crash_dump_observer_android.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "device/geolocation/access_token_store.h"
#include "device/geolocation/geolocation_delegate.h"
#include "device/geolocation/geolocation_provider.h"
#include "net/android/network_change_notifier_factory_android.h"
#include "net/base/network_change_notifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/resource_bundle_android.h"
#include "ui/base/ui_base_paths.h"
#include "ui/gl/gl_surface.h"

namespace android_webview {
namespace {

class AwAccessTokenStore : public device::AccessTokenStore {
 public:
  AwAccessTokenStore() { }

  // device::AccessTokenStore implementation
  void LoadAccessTokens(const LoadAccessTokensCallback& request) override {
    AccessTokenStore::AccessTokenMap access_token_map;
    // AccessTokenMap and net::URLRequestContextGetter not used on Android,
    // but Run needs to be called to finish the geolocation setup.
    request.Run(access_token_map, NULL);
  }
  void SaveAccessToken(const GURL& server_url,
                       const base::string16& access_token) override {}

 private:
  ~AwAccessTokenStore() override {}

  DISALLOW_COPY_AND_ASSIGN(AwAccessTokenStore);
};

// A provider of Geolocation services to override AccessTokenStore.
class AwGeolocationDelegate : public device::GeolocationDelegate {
 public:
  AwGeolocationDelegate() = default;

  scoped_refptr<device::AccessTokenStore> CreateAccessTokenStore() final {
    return new AwAccessTokenStore();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AwGeolocationDelegate);
};

}  // anonymous namespace

AwBrowserMainParts::AwBrowserMainParts(AwContentBrowserClient* browser_client)
    : browser_client_(browser_client) {
}

AwBrowserMainParts::~AwBrowserMainParts() {
}

void AwBrowserMainParts::PreEarlyInitialization() {
  net::NetworkChangeNotifier::SetFactory(new AwNetworkChangeNotifierFactory());

  // Android WebView does not use default MessageLoop. It has its own
  // Android specific MessageLoop. Also see MainMessageLoopRun.
  DCHECK(!main_message_loop_.get());
  main_message_loop_.reset(new base::MessageLoopForUI);
  base::MessageLoopForUI::current()->Start();
}

int AwBrowserMainParts::PreCreateThreads() {
  ui::SetLocalePaksStoredInApk(true);
  std::string locale = ui::ResourceBundle::InitSharedInstanceWithLocale(
      base::android::GetDefaultLocaleString(), NULL,
      ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  if (locale.empty()) {
    LOG(WARNING) << "Failed to load locale .pak from the apk. "
        "Bringing up WebView without any locale";
  }
  base::i18n::SetICUDefaultLocale(locale);

  // Try to directly mmap the resources.pak from the apk. Fall back to load
  // from file, using PATH_SERVICE, otherwise.
  base::FilePath pak_file_path;
  PathService::Get(ui::DIR_RESOURCE_PAKS_ANDROID, &pak_file_path);
  pak_file_path = pak_file_path.AppendASCII("resources.pak");
  ui::LoadMainAndroidPackFile("assets/resources.pak", pak_file_path);

  base::android::MemoryPressureListenerAndroid::RegisterSystemCallback(
      base::android::AttachCurrentThread());
  DeferredGpuCommandService::SetInstance();
  breakpad::CrashDumpObserver::Create();

  if (crash_reporter::IsCrashReporterEnabled()) {
    base::FilePath crash_dir;
    if (PathService::Get(android_webview::DIR_CRASH_DUMPS, &crash_dir)) {
      if (!base::PathExists(crash_dir))
        base::CreateDirectory(crash_dir);
      breakpad::CrashDumpObserver::GetInstance()->RegisterClient(
          base::MakeUnique<breakpad::CrashDumpManager>(
              crash_dir, kAndroidMinidumpDescriptor));
    }
  }

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    // Create the renderers crash manager on the UI thread.
    breakpad::CrashDumpObserver::GetInstance()->RegisterClient(
        base::MakeUnique<AwBrowserTerminator>());
  }

  return content::RESULT_CODE_NORMAL_EXIT;
}

void AwBrowserMainParts::PreMainMessageLoopRun() {
  browser_client_->InitBrowserContext()->PreMainMessageLoopRun();

  device::GeolocationProvider::SetGeolocationDelegate(
      new AwGeolocationDelegate());

  content::RenderFrameHost::AllowInjectingJavaScriptForAndroidWebView();
}

bool AwBrowserMainParts::MainMessageLoopRun(int* result_code) {
  // Android WebView does not use default MessageLoop. It has its own
  // Android specific MessageLoop.
  return true;
}

}  // namespace android_webview
