// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_
#define CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "components/mirroring/mojom/mirroring_service_host.mojom.h"
#include "components/mirroring/mojom/resource_provider.mojom.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/buildflags/buildflags.h"

// TODO(https://crbug.com/879012): Remove the build flag. OffscreenTab should
// not only be defined when extension is enabled.
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/media/offscreen_tab.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace content {
class AudioLoopbackStreamCreator;
class BrowserContext;
class WebContents;
}  // namespace content

namespace mirroring {

// CastMirroringServiceHost starts/stops a mirroring session through Mirroring
// Service, and provides the resources to the Mirroring Service as requested.
//
// TODO(xjz): Adds the implementation to connect to Mirroring Service.
class CastMirroringServiceHost final : public mojom::MirroringServiceHost,
                                       public mojom::ResourceProvider,
#if BUILDFLAG(ENABLE_EXTENSIONS)
                                       public OffscreenTab::Owner,
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
                                       public content::WebContentsObserver {
 public:
  static void GetForTab(content::WebContents* target_contents,
                        mojom::MirroringServiceHostRequest request);

  static void GetForDesktop(content::WebContents* initiator_contents,
                            const std::string& desktop_stream_id,
                            mojom::MirroringServiceHostRequest request);

  static void GetForOffscreenTab(content::BrowserContext* context,
                                 const GURL& presentation_url,
                                 const std::string& presentation_id,
                                 mojom::MirroringServiceHostRequest request);

  // |source_media_id| indicates the mirroring source.
  explicit CastMirroringServiceHost(content::DesktopMediaID source_media_id);

  ~CastMirroringServiceHost() override;

  // mojom::MirroringServiceHost implementation.
  void Start(mojom::SessionParametersPtr session_params,
             mojom::SessionObserverPtr observer,
             mojom::CastMessageChannelPtr outbound_channel,
             mojom::CastMessageChannelRequest inbound_channel) override;

 private:
  friend class CastMirroringServiceHostBrowserTest;

  // ResourceProvider implementation.
  void GetVideoCaptureHost(
      media::mojom::VideoCaptureHostRequest request) override;
  void GetNetworkContext(
      network::mojom::NetworkContextRequest request) override;
  void CreateAudioStream(mojom::AudioStreamCreatorClientPtr client,
                         const media::AudioParameters& params,
                         uint32_t total_segments) override;
  void ConnectToRemotingSource(
      media::mojom::RemoterPtr remoter,
      media::mojom::RemotingSourceRequest request) override;

  // content::WebContentsObserver implementation.
  void WebContentsDestroyed() override;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // OffscreenTab::Owner implementation.
  void RequestMediaAccessPermission(
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  void DestroyTab(OffscreenTab* tab) override;

  // Creates and starts a new OffscreenTab.
  void OpenOffscreenTab(content::BrowserContext* context,
                        const GURL& presentation_url,
                        const std::string& presentation_id);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Describes the media source for this mirroring session.
  content::DesktopMediaID source_media_id_;

  std::unique_ptr<content::AudioLoopbackStreamCreator> audio_stream_creator_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<OffscreenTab> offscreen_tab_;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  DISALLOW_COPY_AND_ASSIGN(CastMirroringServiceHost);
};

}  // namespace mirroring

#endif  // CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_
