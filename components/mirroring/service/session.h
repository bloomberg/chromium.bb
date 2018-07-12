// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_SESSION_H_
#define COMPONENTS_MIRRORING_SERVICE_SESSION_H_

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "components/mirroring/service/interface.h"
#include "components/mirroring/service/media_remoter.h"
#include "components/mirroring/service/message_dispatcher.h"
#include "components/mirroring/service/mirror_settings.h"
#include "components/mirroring/service/rtp_stream.h"
#include "components/mirroring/service/session_monitor.h"
#include "components/mirroring/service/wifi_status_monitor.h"
#include "media/cast/cast_environment.h"
#include "media/cast/net/cast_transport_defines.h"

namespace media {

class AudioInputDevice;

namespace cast {
class CastTransport;
}  // namespace cast

}  // namespace media

namespace mirroring {

struct ReceiverResponse;
class VideoCaptureClient;
class SessionMonitor;

// Controls a mirroring session, including audio/video capturing, Cast
// Streaming, and the switching to/from media remoting. When constructed, it
// does OFFER/ANSWER exchange with the mirroring receiver. Mirroring starts when
// the exchange succeeds and stops when this class is destructed or error
// occurs. |observer| will get notified when status changes. |outbound_channel|
// is responsible for sending messages to the mirroring receiver through Cast
// Channel.
class Session final : public RtpStreamClient, public MediaRemoter::Client {
 public:
  Session(int32_t session_id,
          const CastSinkInfo& sink_info,
          const gfx::Size& max_resolution,
          SessionObserver* observer,
          ResourceProvider* resource_provider,
          CastMessageChannel* outbound_channel);
  // TODO(xjz): Add mojom::CastMessageChannelRequest |inbound_channel| to
  // receive inbound messages.

  ~Session() override;

  // RtpStreamClient implemenation.
  void OnError(const std::string& message) override;
  void RequestRefreshFrame() override;
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

  // Callback for ANSWER response. If the ANSWER is invalid, |observer_| will
  // get notified with error, and session is stopped. Otherwise, capturing and
  // streaming are started with the selected configs.
  void OnAnswer(
      const std::vector<media::cast::FrameSenderConfig>& audio_configs,
      const std::vector<media::cast::FrameSenderConfig>& video_configs,
      const ReceiverResponse& response);

  // Called by |message_dispatcher_| when error occurs while parsing the
  // responses.
  void OnResponseParsingError(const std::string& error_message);

  // Creates an audio input stream through Audio Service. |client| will be
  // called after the stream is created.
  void CreateAudioStream(AudioStreamCreatorClient* client,
                         const media::AudioParameters& params,
                         uint32_t shared_memory_count);

  // Callback for CAPABILITIES_RESPONSE.
  void OnCapabilitiesResponse(const ReceiverResponse& response);

 private:
  // To allow the test code access the |message_dispatcher_|.
  // TODO(xjz): Remove this after adding the inbound message channel argument.
  friend class SessionTest;

  class AudioCapturingCallback;

  // MediaRemoter::Client implementation.
  void ConnectToRemotingSource(
      media::mojom::RemoterPtr remoter,
      media::mojom::RemotingSourceRequest source_request) override;
  void RequestRemotingStreaming() override;
  void RestartMirroringStreaming() override;

  // Stops the current streaming session. If not called from StopSession(), a
  // new streaming session will start later after exchanging OFFER/ANSWER
  // messages with the receiver. This could happen any number of times before
  // StopSession() shuts down everything permanently.
  void StopStreaming();

  void StopSession();  // Shuts down the entire mirroring session.

  // Notify |observer_| that error occurred and close the session.
  void ReportError(SessionError error);

  // Callback by Audio/VideoSender to indicate encoder status change.
  void OnEncoderStatusChange(media::cast::OperationalStatus status);

  // Callback by media::cast::VideoSender to set a new target playout delay.
  void SetTargetPlayoutDelay(base::TimeDelta playout_delay);

  media::VideoEncodeAccelerator::SupportedProfiles GetSupportedVeaProfiles();

  // Create and send OFFER message.
  void CreateAndSendOffer();

  // Send GET_CAPABILITIES message.
  void QueryCapabilitiesForRemoting();

  // Provided by Cast Media Route Provider (MRP).
  const int32_t session_id_;
  const CastSinkInfo sink_info_;

  // State transition:
  // MIRRORING <-------> REMOTING
  //     |                   |
  //     .---> STOPPED <----.
  enum {
    MIRRORING,  // A mirroring streaming session is starting or started.
    REMOTING,   // A remoting streaming session is starting or started.
    STOPPED,    // The session is stopped due to user's request or errors.
  } state_;

  SessionObserver* observer_ = nullptr;
  ResourceProvider* resource_provider_ = nullptr;
  MirrorSettings mirror_settings_;

  MessageDispatcher message_dispatcher_;

  network::mojom::NetworkContextPtr network_context_;

  base::Optional<SessionMonitor> session_monitor_;

  // Created after OFFER/ANSWER exchange succeeds.
  std::unique_ptr<AudioRtpStream> audio_stream_;
  std::unique_ptr<VideoRtpStream> video_stream_;
  std::unique_ptr<VideoCaptureClient> video_capture_client_;
  scoped_refptr<media::cast::CastEnvironment> cast_environment_ = nullptr;
  std::unique_ptr<media::cast::CastTransport> cast_transport_;
  scoped_refptr<base::SingleThreadTaskRunner> audio_encode_thread_ = nullptr;
  scoped_refptr<base::SingleThreadTaskRunner> video_encode_thread_ = nullptr;
  std::unique_ptr<AudioCapturingCallback> audio_capturing_callback_;
  scoped_refptr<media::AudioInputDevice> audio_input_device_;
  std::unique_ptr<MediaRemoter> media_remoter_;

  base::WeakPtrFactory<Session> weak_factory_;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_SESSION_H_
