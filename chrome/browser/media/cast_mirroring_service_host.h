// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_
#define CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_

#include "base/macros.h"
#include "components/mirroring/mojom/mirroring_service_host.mojom.h"
#include "components/mirroring/mojom/resource_provider.mojom.h"
#include "content/public/browser/desktop_media_id.h"

namespace content {
class AudioLoopbackStreamCreator;
}  // namespace content

namespace mirroring {

// CastMirroringServiceHost starts/stops a mirroring session through Mirroring
// Service, and provides the resources to the Mirroring Service as requested.
//
// TODO(xjz): Adds the implementation to connect to Mirroring Service.
class CastMirroringServiceHost final : public mojom::MirroringServiceHost,
                                       public mojom::ResourceProvider {
 public:
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

  // Describes the media source for this mirroring session.
  const content::DesktopMediaID source_media_id_;

  std::unique_ptr<content::AudioLoopbackStreamCreator> audio_stream_creator_;

  DISALLOW_COPY_AND_ASSIGN(CastMirroringServiceHost);
};

}  // namespace mirroring

#endif  // CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_
