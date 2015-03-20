// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/lib/main/aw_main_delegate.h"

#include "android_webview/browser/aw_content_browser_client.h"
#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/scoped_allow_wait_for_legacy_web_view_api.h"
#include "android_webview/crash_reporter/aw_microdump_crash_reporter.h"
#include "android_webview/lib/aw_browser_dependency_factory_impl.h"
#include "android_webview/native/aw_media_url_interceptor.h"
#include "android_webview/native/aw_message_port_service_impl.h"
#include "android_webview/native/aw_quota_manager_bridge_impl.h"
#include "android_webview/native/aw_web_contents_view_delegate.h"
#include "android_webview/native/aw_web_preferences_populater_impl.h"
#include "android_webview/native/external_video_surface_container_impl.h"
#include "android_webview/native/public/aw_assets.h"
#include "android_webview/renderer/aw_content_renderer_client.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/i18n/icu_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_restrictions.h"
#include "cc/base/switches.h"
#include "content/public/browser/android/browser_media_player_manager.h"
#include "content/public/browser/browser_main_runner.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_descriptors.h"
#include "content/public/common/content_switches.h"
#include "gin/public/isolate_holder.h"
#include "gpu/command_buffer/client/gl_in_process_context.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "media/base/media_switches.h"

namespace android_webview {

namespace {

// TODO(boliu): Remove this global Allow once the underlying issues are
// resolved - http://crbug.com/240453. See AwMainDelegate::RunProcess below.
base::LazyInstance<scoped_ptr<ScopedAllowWaitForLegacyWebViewApi> >
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

  base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  cl->AppendSwitch(switches::kEnableBeginFrameScheduling);

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

#if defined(VIDEO_HOLE)
  // Support EME with hole-punching. For example, Widevine L1.
  cl->AppendSwitch(switches::kMediaDrmEnableNonCompositing);
  cl->AppendSwitch(switches::kDisableEncryptedMedia);
#endif

  // WebRTC hardware decoding is not supported, internal bug 15075307
  cl->AppendSwitch(switches::kDisableWebRtcHWDecoding);
  cl->AppendSwitch(switches::kDisableAcceleratedVideoDecode);

  // This is needed for sharing textures across the different GL threads.
  cl->AppendSwitch(switches::kEnableThreadedTextureMailboxes);

  // This is needed to be able to mmap the V8 snapshot and ICU data file
  // directly from the WebView .apk.
  // This needs to be here so that it gets to run before the code in
  // content_main_runner that reads these values tries to do so.
  // In multi-process mode this code would live in
  // AwContentBrowserClient::GetAdditionalMappedFilesForChildProcess.
#ifdef V8_USE_EXTERNAL_STARTUP_DATA
#ifdef __LP64__
  const char kNativesFileName[] = "natives_blob_64.bin";
  const char kSnapshotFileName[] = "snapshot_blob_64.bin";
#else
  const char kNativesFileName[] = "natives_blob_32.bin";
  const char kSnapshotFileName[] = "snapshot_blob_32.bin";
#endif // __LP64__
  // TODO(gsennton) we should use
  // gin::IsolateHolder::kNativesFileName/kSnapshotFileName
  // here when those files have arch specific names http://crbug.com/455699
  CHECK(AwAssets::RegisterAssetWithGlobalDescriptors(
      kV8NativesDataDescriptor, kNativesFileName));
  CHECK(AwAssets::RegisterAssetWithGlobalDescriptors(
      kV8SnapshotDataDescriptor, kSnapshotFileName));
#endif
  // TODO(mkosiba): make this CHECK when the android_webview_build uses an asset
  // from the .apk too.
  AwAssets::RegisterAssetWithGlobalDescriptors(
      kAndroidICUDataDescriptor, base::i18n::kIcuDataFileName);

  return false;
}

void AwMainDelegate::PreSandboxStartup() {
  // TODO(torne): When we have a separate renderer process, we need to handle
  // being passed open FDs for the resource paks here.
#if defined(ARCH_CPU_ARM_FAMILY)
  // Create an instance of the CPU class to parse /proc/cpuinfo and cache
  // cpu_brand info.
  base::CPU cpu_info;
#endif

  crash_reporter::EnableMicrodumpCrashReporter();
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
    DCHECK_LT(exit_code, 0);

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

#if defined(VIDEO_HOLE)
content::ExternalVideoSurfaceContainer*
AwMainDelegate::CreateExternalVideoSurfaceContainer(
    content::WebContents* web_contents) {
  return new ExternalVideoSurfaceContainerImpl(web_contents);
}
#endif

}  // namespace android_webview
