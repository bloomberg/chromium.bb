// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/main/aw_main_delegate.h"

#include <memory>

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/deferred_gpu_command_service.h"
#include "android_webview/browser/scoped_allow_wait_for_legacy_web_view_api.h"
#include "android_webview/common/aw_descriptors.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/crash_reporter/aw_microdump_crash_reporter.h"
#include "android_webview/gpu/aw_content_gpu_client.h"
#include "android_webview/lib/aw_browser_dependency_factory_impl.h"
#include "android_webview/native/aw_locale_manager_impl.h"
#include "android_webview/native/aw_media_url_interceptor.h"
#include "android_webview/native/aw_message_port_service_impl.h"
#include "android_webview/native/aw_quota_manager_bridge_impl.h"
#include "android_webview/native/aw_web_contents_view_delegate.h"
#include "android_webview/native/aw_web_preferences_populater_impl.h"
#include "android_webview/renderer/aw_content_renderer_client.h"
#include "base/android/apk_assets.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "cc/base/switches.h"
#include "components/external_video_surface/browser/android/external_video_surface_container_impl.h"
#include "content/public/browser/android/browser_media_player_manager_register.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "gin/public/isolate_holder.h"
#include "gin/v8_initializer.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/base/media_switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

namespace android_webview {

namespace {

// TODO(boliu): Remove this global Allow once the underlying issues are
// resolved - http://crbug.com/240453. See AwMainDelegate::RunProcess below.
base::LazyInstance<std::unique_ptr<ScopedAllowWaitForLegacyWebViewApi>>
    g_allow_wait_in_ui_thread = LAZY_INSTANCE_INITIALIZER;
}

AwMainDelegate::AwMainDelegate() {
}

AwMainDelegate::~AwMainDelegate() {
}

bool AwMainDelegate::BasicStartupComplete(int* exit_code) {
  content::SetContentClient(&content_client_);

  content::RegisterMediaUrlInterceptor(new AwMediaUrlInterceptor());

  BrowserViewRenderer::CalculateTileMemoryPolicy();

  // WebView apps can override WebView#computeScroll to achieve custom
  // scroll/fling. As a result, fling animations may not be ticked, potentially
  // confusing the tap suppression controller. Simply disable it for WebView.
  ui::GestureConfiguration::GetInstance()
      ->set_fling_touchscreen_tap_suppression_enabled(false);

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(cc::switches::kEnableBeginFrameScheduling);

  // WebView uses the Android system's scrollbars and overscroll glow.
  cl->AppendSwitch(switches::kDisableOverscrollEdgeEffect);

  // Pull-to-refresh should never be a default WebView action.
  cl->AppendSwitch(switches::kDisablePullToRefreshEffect);

  // Not yet supported in single-process mode.
  cl->AppendSwitch(switches::kDisableSharedWorkers);

  // File system API not supported (requires some new API; internal bug 6930981)
  cl->AppendSwitch(switches::kDisableFileSystem);

  // Web Notification API and the Push API are not supported (crbug.com/434712)
  cl->AppendSwitch(switches::kDisableNotifications);

  // WebRTC hardware decoding is not supported, internal bug 15075307
#if defined(ENABLE_WEBRTC)
  cl->AppendSwitch(switches::kDisableWebRtcHWDecoding);
#endif

  // This is needed for sharing textures across the different GL threads.
  cl->AppendSwitch(switches::kEnableThreadedTextureMailboxes);

  // WebView does not yet support screen orientation locking.
  cl->AppendSwitch(switches::kDisableScreenOrientationLock);

  // WebView does not currently support Web Speech API (crbug.com/487255)
  cl->AppendSwitch(switches::kDisableSpeechAPI);

  // WebView does not currently support the Permissions API (crbug.com/490120)
  cl->AppendSwitch(switches::kDisablePermissionsAPI);

  // WebView does not (yet) save Chromium data during shutdown, so add setting
  // for Chrome to aggressively persist DOM Storage to minimize data loss.
  // http://crbug.com/479767
  cl->AppendSwitch(switches::kEnableAggressiveDOMStorageFlushing);

  // Webview does not currently support the Presentation API, see
  // https://crbug.com/521319
  cl->AppendSwitch(switches::kDisablePresentationAPI);

  if (cl->GetSwitchValueASCII(switches::kProcessType).empty()) {
    // Browser process (no type specified).

    base::android::RegisterApkAssetWithGlobalDescriptors(
        kV8NativesDataDescriptor32,
        gin::V8Initializer::GetNativesFilePath(true).AsUTF8Unsafe());
    base::android::RegisterApkAssetWithGlobalDescriptors(
        kV8SnapshotDataDescriptor32,
        gin::V8Initializer::GetSnapshotFilePath(true).AsUTF8Unsafe());

    base::android::RegisterApkAssetWithGlobalDescriptors(
        kV8NativesDataDescriptor64,
        gin::V8Initializer::GetNativesFilePath(false).AsUTF8Unsafe());
    base::android::RegisterApkAssetWithGlobalDescriptors(
        kV8SnapshotDataDescriptor64,
        gin::V8Initializer::GetSnapshotFilePath(false).AsUTF8Unsafe());
  }

  if (cl->HasSwitch(switches::kWebViewSandboxedRenderer)) {
    cl->AppendSwitch(switches::kInProcessGPU);
    cl->AppendSwitchASCII(switches::kRendererProcessLimit, "1");
    cl->AppendSwitch(switches::kDisableRendererBackgrounding);
  }

  // TODO(liberato, watk): Reenable after resolving fullscreen test failures.
  // See http://crbug.com/597495
  cl->AppendSwitch(switches::kDisableUnifiedMediaPipeline);

  return false;
}

void AwMainDelegate::PreSandboxStartup() {
#if defined(ARCH_CPU_ARM_FAMILY)
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache
  // cpu_brand info.
  base::CPU cpu_info;
#endif

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  std::string process_type =
      command_line.GetSwitchValueASCII(switches::kProcessType);
  int crash_signal_fd = -1;
  if (process_type == switches::kRendererProcess) {
    auto global_descriptors = base::GlobalDescriptors::GetInstance();
    int pak_fd = global_descriptors->Get(kAndroidWebViewLocalePakDescriptor);
    base::MemoryMappedFile::Region pak_region =
        global_descriptors->GetRegion(kAndroidWebViewLocalePakDescriptor);
    ResourceBundle::InitSharedInstanceWithPakFileRegion(base::File(pak_fd),
                                                        pak_region);
    pak_fd = global_descriptors->Get(kAndroidWebViewMainPakDescriptor);
    pak_region =
        global_descriptors->GetRegion(kAndroidWebViewMainPakDescriptor);
    ResourceBundle::GetSharedInstance().AddDataPackFromFileRegion(
        base::File(pak_fd), pak_region, ui::SCALE_FACTOR_NONE);
    crash_signal_fd =
        global_descriptors->Get(kAndroidWebViewCrashSignalDescriptor);
  }
  if (process_type.empty() &&
      command_line.HasSwitch(switches::kSingleProcess)) {
    // "webview" has a special treatment in breakpad_linux.cc.
    process_type = "webview";
  }

  crash_reporter::EnableMicrodumpCrashReporter(process_type, crash_signal_fd);
}

int AwMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    AwBrowserDependencyFactoryImpl::InstallInstance();

    browser_runner_.reset(content::BrowserMainRunner::Create());
    int exit_code = browser_runner_->Initialize(main_function_params);
    DCHECK_LT(exit_code, 0);

    // At this point the content client has received the GPU info required
    // to create a GPU fingerpring, and we can pass it to the microdump
    // crash handler on the same thread as the crash handler was initialized.
    crash_reporter::AddGpuFingerprintToMicrodumpCrashHandler(
        content_client_.gpu_fingerprint());

    g_allow_wait_in_ui_thread.Get().reset(
        new ScopedAllowWaitForLegacyWebViewApi);

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
  content_browser_client_.reset(new AwContentBrowserClient(this));
  return content_browser_client_.get();
}

namespace {
gpu::SyncPointManager* GetSyncPointManager() {
  DCHECK(DeferredGpuCommandService::GetInstance());
  return DeferredGpuCommandService::GetInstance()->sync_point_manager();
}
}  // namespace

content::ContentGpuClient* AwMainDelegate::CreateContentGpuClient() {
  content_gpu_client_.reset(
      new AwContentGpuClient(base::Bind(&GetSyncPointManager)));
  return content_gpu_client_.get();
}

content::ContentRendererClient*
    AwMainDelegate::CreateContentRendererClient() {
  content_renderer_client_.reset(new AwContentRendererClient());
  return content_renderer_client_.get();
}

scoped_refptr<AwQuotaManagerBridge> AwMainDelegate::CreateAwQuotaManagerBridge(
    AwBrowserContext* browser_context) {
  return AwQuotaManagerBridgeImpl::Create(browser_context);
}

content::WebContentsViewDelegate* AwMainDelegate::CreateViewDelegate(
    content::WebContents* web_contents) {
  return AwWebContentsViewDelegate::Create(web_contents);
}

AwWebPreferencesPopulater* AwMainDelegate::CreateWebPreferencesPopulater() {
  return new AwWebPreferencesPopulaterImpl();
}

AwMessagePortService* AwMainDelegate::CreateAwMessagePortService() {
  return new AwMessagePortServiceImpl();
}

AwLocaleManager* AwMainDelegate::CreateAwLocaleManager() {
  return new AwLocaleManagerImpl();
}

#if defined(VIDEO_HOLE)
content::ExternalVideoSurfaceContainer*
AwMainDelegate::CreateExternalVideoSurfaceContainer(
    content::WebContents* web_contents) {
  return external_video_surface::ExternalVideoSurfaceContainerImpl::Create(
      web_contents);
}
#endif

}  // namespace android_webview
