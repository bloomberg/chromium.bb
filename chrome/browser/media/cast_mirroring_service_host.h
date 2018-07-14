// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_
#define CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_

#include "base/macros.h"
#include "components/mirroring/service/interface.h"
#include "content/public/browser/desktop_media_id.h"

namespace mirroring {

// CastMirroringServiceHost starts/stops a mirroring session through Mirroring
// Service, and provides the resources to the Mirroring Service as requested.
//
// TODO(xjz): Implements the mojo interface and adds the implementation to
// connect to Mirroring Service.
class CastMirroringServiceHost final : public ResourceProvider {
 public:
  explicit CastMirroringServiceHost(content::DesktopMediaID source_media_id);

  ~CastMirroringServiceHost() override;

  // Connect to Mirroring Service to start a mirroring session.
  // TODO(xjz): Add mojom::CastMessageChannelRequest argument for inbound
  // message handling.
  void Start(int32_t session_id,
             const CastSinkInfo& sink_info,
             CastMessageChannel* outbound_channel,
             SessionObserver* observer);

  // TODO(xjz): Add interfaces to get the streaming events/stats and mirroring
  // logs.

 private:
  // ResourceProvider implementation.
  void GetVideoCaptureHost(
      media::mojom::VideoCaptureHostRequest request) override;
  void GetNetworkContext(
      network::mojom::NetworkContextRequest request) override;
  void CreateAudioStream(AudioStreamCreatorClient* client,
                         const media::AudioParameters& params,
                         uint32_t total_segments) override;
  void ConnectToRemotingSource(
      media::mojom::RemoterPtr remoter,
      media::mojom::RemotingSourceRequest request) override;

  // Describes the media source for this mirroring session.
  const content::DesktopMediaID source_media_id_;

  DISALLOW_COPY_AND_ASSIGN(CastMirroringServiceHost);
};

}  // namespace mirroring

#endif  // CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_
