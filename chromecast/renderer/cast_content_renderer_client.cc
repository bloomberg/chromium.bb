// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/cast_content_renderer_client.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/common/media/cast_media_client.h"
#include "chromecast/crash/cast_crash_keys.h"
#include "chromecast/renderer/cast_media_load_deferrer.h"
#include "chromecast/renderer/cast_render_thread_observer.h"
#include "chromecast/renderer/key_systems_cast.h"
#include "chromecast/renderer/media/chromecast_media_renderer_factory.h"
#include "chromecast/renderer/media/media_caps_observer_impl.h"
#include "components/network_hints/renderer/prescient_networking_dispatcher.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "media/base/media.h"
#include "services/shell/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#endif  // OS_ANDROID

namespace chromecast {
namespace shell {

namespace {

// Default background color to set for WebViews. WebColor is in ARGB format
// though the comment of WebColor says it is in RGBA.
const blink::WebColor kColorBlack = 0xFF000000;

}  // namespace

CastContentRendererClient::CastContentRendererClient()
    : allow_hidden_media_playback_(
          base::CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kAllowHiddenMediaPlayback)) {
#if defined(OS_ANDROID)
  DCHECK(::media::MediaCodecUtil::IsMediaCodecAvailable())
      << "MediaCodec is not available!";
  // Platform decoder support must be enabled before we set the
  // IsCodecSupportedCB because the latter instantiates the lazy MimeUtil
  // instance, which caches the platform decoder supported state when it is
  // constructed.
  ::media::EnablePlatformDecoderSupport();
#endif  // OS_ANDROID
}

CastContentRendererClient::~CastContentRendererClient() {
}

void CastContentRendererClient::RenderThreadStarted() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Register as observer for media capabilities
  content::RenderThread* thread = content::RenderThread::Get();
  media::mojom::MediaCapsPtr media_caps;
  thread->GetRemoteInterfaces()->GetInterface(&media_caps);
  media::mojom::MediaCapsObserverPtr proxy;
  media_caps_observer_.reset(new media::MediaCapsObserverImpl(&proxy));
  media_caps->AddObserver(std::move(proxy));

  chromecast::media::CastMediaClient::Initialize();

  cast_observer_.reset(new CastRenderThreadObserver());

  prescient_networking_dispatcher_.reset(
      new network_hints::PrescientNetworkingDispatcher());

  std::string last_launched_app =
      command_line->GetSwitchValueNative(switches::kLastLaunchedApp);
  if (!last_launched_app.empty())
    base::debug::SetCrashKeyValue(crash_keys::kLastApp, last_launched_app);

  std::string previous_app =
      command_line->GetSwitchValueNative(switches::kPreviousApp);
  if (!previous_app.empty())
    base::debug::SetCrashKeyValue(crash_keys::kPreviousApp, previous_app);
}

void CastContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  blink::WebView* webview = render_view->GetWebView();
  if (webview) {
    blink::WebFrameWidget* web_frame_widget = render_view->GetWebFrameWidget();
    web_frame_widget->setBaseBackgroundColor(kColorBlack);

    // Settings for ATV (Android defaults are not what we want):
    webview->settings()->setMediaControlsOverlayPlayButtonEnabled(false);

    // Disable application cache as Chromecast doesn't support off-line
    // application running.
    webview->settings()->setOfflineWebApplicationCacheEnabled(false);
  }
}

void CastContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>*
        key_systems_properties) {
  AddChromecastKeySystems(key_systems_properties, false);
}

#if !defined(OS_ANDROID)
std::unique_ptr<::media::RendererFactory>
CastContentRendererClient::CreateMediaRendererFactory(
    ::content::RenderFrame* render_frame,
    ::media::GpuVideoAcceleratorFactories* gpu_factories,
    const scoped_refptr<::media::MediaLog>& media_log) {
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kEnableCmaMediaPipeline))
    return nullptr;

  return std::unique_ptr<::media::RendererFactory>(
      new chromecast::media::ChromecastMediaRendererFactory(
          gpu_factories, render_frame->GetRoutingID()));
}
#endif

blink::WebPrescientNetworking*
CastContentRendererClient::GetPrescientNetworking() {
  return prescient_networking_dispatcher_.get();
}

void CastContentRendererClient::DeferMediaLoad(
    content::RenderFrame* render_frame,
    bool render_frame_has_played_media_before,
    const base::Closure& closure) {
  if (!render_frame->IsHidden() || allow_hidden_media_playback_) {
    closure.Run();
    return;
  }

  // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
  new CastMediaLoadDeferrer(render_frame, closure);
}

}  // namespace shell
}  // namespace chromecast
