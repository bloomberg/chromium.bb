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
#include "chromecast/media/base/media_caps.h"
#include "chromecast/media/base/media_codec_support.h"
#include "chromecast/media/base/supported_codec_profile_levels_memo.h"
#include "chromecast/public/media/media_capabilities_shlib.h"
#include "chromecast/renderer/cast_render_frame_action_deferrer.h"
#include "chromecast/renderer/media/key_systems_cast.h"
#include "chromecast/renderer/media/media_caps_observer_impl.h"
#include "components/network_hints/renderer/prescient_networking_dispatcher.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "media/base/media.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/WebKit/public/platform/WebColor.h"
#include "third_party/WebKit/public/platform/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebSettings.h"
#include "third_party/WebKit/public/web/WebView.h"

#if defined(OS_ANDROID)
#include "media/base/android/media_codec_util.h"
#else
#include "chromecast/renderer/memory_pressure_observer_impl.h"
#endif  // OS_ANDROID

#if !defined(OS_FUCHSIA)
#include "chromecast/crash/cast_crash_keys.h"
#endif  // !defined(OS_FUCHSIA)

namespace chromecast {
namespace shell {

namespace {

// Default background color to set for WebViews. WebColor is in ARGB format
// though the comment of WebColor says it is in RGBA.
const blink::WebColor kColorBlack = 0xFF000000;

}  // namespace

CastContentRendererClient::CastContentRendererClient()
    : supported_profiles_(new media::SupportedCodecProfileLevelsMemo()),
      allow_hidden_media_playback_(
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
  // Register as observer for media capabilities
  content::RenderThread* thread = content::RenderThread::Get();
  media::mojom::MediaCapsPtr media_caps;
  thread->GetConnector()->BindInterface(content::mojom::kBrowserServiceName,
                                        &media_caps);
  media::mojom::MediaCapsObserverPtr proxy;
  media_caps_observer_.reset(
      new media::MediaCapsObserverImpl(&proxy, supported_profiles_.get()));
  media_caps->AddObserver(std::move(proxy));

#if !defined(OS_ANDROID)
  // Register to observe memory pressure changes
  mojom::MemoryPressureControllerPtr memory_pressure_controller;
  thread->GetConnector()->BindInterface(content::mojom::kBrowserServiceName,
                                        &memory_pressure_controller);
  mojom::MemoryPressureObserverPtr memory_pressure_proxy;
  memory_pressure_observer_.reset(
      new MemoryPressureObserverImpl(&memory_pressure_proxy));
  memory_pressure_controller->AddObserver(std::move(memory_pressure_proxy));
#endif

  prescient_networking_dispatcher_.reset(
      new network_hints::PrescientNetworkingDispatcher());

#if !defined(OS_FUCHSIA)
  // TODO(crbug.com/753619): Enable crash reporting on Fuchsia.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  std::string last_launched_app =
      command_line->GetSwitchValueNative(switches::kLastLaunchedApp);
  if (!last_launched_app.empty())
    base::debug::SetCrashKeyValue(crash_keys::kLastApp, last_launched_app);

  std::string previous_app =
      command_line->GetSwitchValueNative(switches::kPreviousApp);
  if (!previous_app.empty())
    base::debug::SetCrashKeyValue(crash_keys::kPreviousApp, previous_app);
#endif  // !defined(OS_FUCHSIA)
}

void CastContentRendererClient::RenderViewCreated(
    content::RenderView* render_view) {
  blink::WebView* webview = render_view->GetWebView();
  if (webview) {
    blink::WebFrameWidget* web_frame_widget = render_view->GetWebFrameWidget();
    web_frame_widget->SetBaseBackgroundColor(kColorBlack);

    // Disable application cache as Chromecast doesn't support off-line
    // application running.
    webview->GetSettings()->SetOfflineWebApplicationCacheEnabled(false);
  }
}

void CastContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<::media::KeySystemProperties>>*
        key_systems_properties) {
  media::AddChromecastKeySystems(key_systems_properties,
                                 false /* enable_persistent_license_support */,
                                 false /* force_software_crypto */);
}

bool CastContentRendererClient::IsSupportedAudioConfig(
    const ::media::AudioConfig& config) {
#if defined(OS_ANDROID)
  media::AudioCodec codec = media::ToCastAudioCodec(config.codec);

  // No ATV device we know of has (E)AC3 decoder, so it relies on the audio sink
  // device.
  if (codec == media::kCodecEAC3)
    return media::MediaCapabilities::HdmiSinkSupportsEAC3();
  if (codec == media::kCodecAC3)
    return media::MediaCapabilities::HdmiSinkSupportsAC3();

  // TODO(sanfin): Implement this for Android.
  return true;
#else
  media::AudioCodec codec = media::ToCastAudioCodec(config.codec);
  // Cast platform implements software decoding of Opus and FLAC, so only PCM
  // support is necessary in order to support Opus and FLAC.
  if (codec == media::kCodecOpus || codec == media::kCodecFLAC)
    codec = media::kCodecPCM;

  // If HDMI sink supports AC3/EAC3 codecs then we don't need the vendor backend
  // to support these codec directly.
  if (codec == media::kCodecEAC3 &&
      media::MediaCapabilities::HdmiSinkSupportsEAC3())
    return true;
  if (codec == media::kCodecAC3 &&
      media::MediaCapabilities::HdmiSinkSupportsAC3())
    return true;

  media::AudioConfig cast_audio_config;
  cast_audio_config.codec = codec;
  return media::MediaCapabilitiesShlib::IsSupportedAudioConfig(
      cast_audio_config);
#endif
}

bool CastContentRendererClient::IsSupportedVideoConfig(
    const ::media::VideoConfig& config) {
// TODO(servolk): make use of eotf.
#if defined(OS_ANDROID)
  return supported_profiles_->IsSupportedVideoConfig(
      media::ToCastVideoCodec(config.codec, config.profile),
      media::ToCastVideoProfile(config.profile), config.level);
#else
  return media::MediaCapabilitiesShlib::IsSupportedVideoConfig(
      media::ToCastVideoCodec(config.codec, config.profile),
      media::ToCastVideoProfile(config.profile), config.level);
#endif
}

bool CastContentRendererClient::IsSupportedBitstreamAudioCodec(
    ::media::AudioCodec codec) {
  return (codec == ::media::kCodecAC3 &&
          media::MediaCapabilities::HdmiSinkSupportsAC3()) ||
         (codec == ::media::kCodecEAC3 &&
          media::MediaCapabilities::HdmiSinkSupportsEAC3());
}

blink::WebPrescientNetworking*
CastContentRendererClient::GetPrescientNetworking() {
  return prescient_networking_dispatcher_.get();
}

void CastContentRendererClient::DeferMediaLoad(
    content::RenderFrame* render_frame,
    bool render_frame_has_played_media_before,
    const base::Closure& closure) {
  if (allow_hidden_media_playback_) {
    closure.Run();
    return;
  }

  RunWhenInForeground(render_frame, closure);
}

void CastContentRendererClient::RunWhenInForeground(
    content::RenderFrame* render_frame,
    const base::Closure& closure) {
  if (!render_frame->IsHidden()) {
    closure.Run();
    return;
  }

  // Lifetime is tied to |render_frame| via content::RenderFrameObserver.
  new CastRenderFrameActionDeferrer(render_frame, closure);
}

bool CastContentRendererClient::AllowIdleMediaSuspend() {
  return false;
}

void CastContentRendererClient::
    SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() {
  // Settings for ATV (Android defaults are not what we want).
  blink::WebRuntimeFeatures::EnableMediaControlsOverlayPlayButton(false);
}

}  // namespace shell
}  // namespace chromecast
