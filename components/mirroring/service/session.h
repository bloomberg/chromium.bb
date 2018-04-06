// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_SESSION_H_
#define COMPONENTS_MIRRORING_SERVICE_SESSION_H_

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/mirroring/service/interface.h"
#include "components/mirroring/service/rtp_stream.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport_defines.h"

namespace media {

namespace cast {
class CastTransport;
}  // namespace cast

}  // namespace media

namespace mirroring {

class VideoCaptureClient;

class Session final : public RtpStreamClient {
 public:
  Session(SessionType session_type,
          const net::IPEndPoint& receiver_endpoint,
          SessionClient* client);
  ~Session() override;

  // RtpStreamClient implemenation.
  void OnError(const std::string& message) override;
  void RequestRefreshFrame() override;
  media::VideoEncodeAccelerator::SupportedProfiles
  GetSupportedVideoEncodeAcceleratorProfiles() override;
  void CreateVideoEncodeAccelerator(
      const media::cast::ReceiveVideoEncodeAcceleratorCallback& callback)
      override;
  void CreateVideoEncodeMemory(
      size_t size,
      const media::cast::ReceiveVideoEncodeMemoryCallback& callback) override;

  // Callbacks by media::cast::CastTransport::Client.
  void OnTransportStatusChanged(media::cast::CastTransportStatus status);
  void OnLoggingEventsReceived(
      std::unique_ptr<std::vector<media::cast::FrameEvent>> frame_events,
      std::unique_ptr<std::vector<media::cast::PacketEvent>> packet_events);

 private:
  // Callback when OFFER/ANSWER message exchange finishes. Starts a mirroing
  // session.
  void StartInternal(const net::IPEndPoint& receiver_endpoint,
                     const media::cast::FrameSenderConfig& audio_config,
                     const media::cast::FrameSenderConfig& video_config);

  void StopSession();

  // Notify |client_| that error occurred and close the session.
  void ReportError(SessionError error);

  // Callback by Audio/VideoSender to indicate encoder status change.
  void OnEncoderStatusChange(media::cast::OperationalStatus status);

  // Callback by media::cast::VideoSender to set a new target playout delay.
  void SetTargetPlayoutDelay(base::TimeDelta playout_delay);

  // Callback by |start_timeout_timer_|.
  void OnOfferAnswerExchangeTimeout();

  SessionClient* client_ = nullptr;

  // Create on StartInternal().
  std::unique_ptr<AudioRtpStream> audio_stream_;
  std::unique_ptr<VideoRtpStream> video_stream_;
  std::unique_ptr<VideoCaptureClient> video_capture_client_;
  scoped_refptr<media::cast::CastEnvironment> cast_environment_ = nullptr;
  std::unique_ptr<media::cast::CastTransport> cast_transport_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_encode_thread_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> video_encode_thread_ = nullptr;

  // Fire if the OFFER/ANSWER exchange times out.
  base::OneShotTimer start_timeout_timer_;

  base::WeakPtrFactory<Session> weak_factory_;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_SESSION_H_
