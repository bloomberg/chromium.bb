// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
#define CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "chromecast/chromecast_features.h"
#include "content/public/renderer/content_renderer_client.h"

#if defined(CHROMECAST_BUILD)
#include <string>
#endif

namespace extensions {
class ExtensionsClient;
class ExtensionsGuestViewContainerDispatcher;
class CastExtensionsRendererClient;
}  // namespace extensions

namespace network_hints {
class PrescientNetworkingDispatcher;
}  // namespace network_hints

namespace chromecast {
class MemoryPressureObserverImpl;
namespace media {
class MediaCapsObserverImpl;
class SupportedCodecProfileLevelsMemo;
}

namespace shell {

class CastContentRendererClient : public content::ContentRendererClient {
 public:
  // Creates an implementation of CastContentRendererClient. Platform should
  // link in an implementation as needed.
  static std::unique_ptr<CastContentRendererClient> Create();

  ~CastContentRendererClient() override;

  // ContentRendererClient implementation:
  void RenderThreadStarted() override;
  void RenderViewCreated(content::RenderView* render_view) override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  content::BrowserPluginDelegate* CreateBrowserPluginDelegate(
      content::RenderFrame* render_frame,
      const std::string& mime_type,
      const GURL& original_url) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;
  void AddSupportedKeySystems(
      std::vector<std::unique_ptr<::media::KeySystemProperties>>*
          key_systems_properties) override;
  bool IsSupportedAudioConfig(const ::media::AudioConfig& config) override;
  bool IsSupportedVideoConfig(const ::media::VideoConfig& config) override;
  bool IsSupportedBitstreamAudioCodec(::media::AudioCodec codec) override;
  blink::WebPrescientNetworking* GetPrescientNetworking() override;
  void DeferMediaLoad(content::RenderFrame* render_frame,
                      bool render_frame_has_played_media_before,
                      const base::Closure& closure) override;
  bool AllowIdleMediaSuspend() override;
  void SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() override;

 protected:
  CastContentRendererClient();

  virtual void RunWhenInForeground(content::RenderFrame* render_frame,
                                   const base::Closure& closure);

 private:
  std::unique_ptr<network_hints::PrescientNetworkingDispatcher>
      prescient_networking_dispatcher_;
  std::unique_ptr<media::MediaCapsObserverImpl> media_caps_observer_;
  std::unique_ptr<media::SupportedCodecProfileLevelsMemo> supported_profiles_;
#if !defined(OS_ANDROID)
  std::unique_ptr<MemoryPressureObserverImpl> memory_pressure_observer_;
#endif

#if BUILDFLAG(ENABLE_CHROMECAST_EXTENSIONS)
  std::unique_ptr<extensions::ExtensionsClient> extensions_client_;
  std::unique_ptr<extensions::CastExtensionsRendererClient>
      extensions_renderer_client_;
  std::unique_ptr<extensions::ExtensionsGuestViewContainerDispatcher>
      guest_view_container_dispatcher_;
#endif

  const bool allow_hidden_media_playback_;

  DISALLOW_COPY_AND_ASSIGN(CastContentRendererClient);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_CAST_CONTENT_RENDERER_CLIENT_H_
