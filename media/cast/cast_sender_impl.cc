// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/cast/cast_sender_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/base/video_frame.h"

namespace media {
namespace cast {

// The LocalFrameInput class posts all incoming frames; audio and video to the
// main cast thread for processing.
// This make the cast sender interface thread safe.
class LocalFrameInput : public FrameInput {
 public:
  LocalFrameInput(scoped_refptr<CastEnvironment> cast_environment,
                  base::WeakPtr<AudioSender> audio_sender,
                  base::WeakPtr<VideoSender> video_sender)
      : cast_environment_(cast_environment),
        audio_sender_(audio_sender),
        video_sender_(video_sender) {}

  virtual void InsertRawVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& capture_time) OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&VideoSender::InsertRawVideoFrame,
                                           video_sender_,
                                           video_frame,
                                           capture_time));
  }

  virtual void InsertAudio(const AudioBus* audio_bus,
                           const base::TimeTicks& recorded_time,
                           const base::Closure& done_callback) OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&AudioSender::InsertAudio,
                                           audio_sender_,
                                           audio_bus,
                                           recorded_time,
                                           done_callback));
  }

 protected:
  virtual ~LocalFrameInput() {}

 private:
  friend class base::RefCountedThreadSafe<LocalFrameInput>;

  scoped_refptr<CastEnvironment> cast_environment_;
  base::WeakPtr<AudioSender> audio_sender_;
  base::WeakPtr<VideoSender> video_sender_;

  DISALLOW_COPY_AND_ASSIGN(LocalFrameInput);
};

CastSender* CastSender::CreateCastSender(
    scoped_refptr<CastEnvironment> cast_environment,
    const AudioSenderConfig* audio_config,
    const VideoSenderConfig* video_config,
    const scoped_refptr<GpuVideoAcceleratorFactories>& gpu_factories,
    const CastInitializationCallback& initialization_status,
    transport::CastTransportSender* const transport_sender) {
  CHECK(cast_environment);
  return new CastSenderImpl(cast_environment,
                            audio_config,
                            video_config,
                            gpu_factories,
                            initialization_status,
                            transport_sender);
}

CastSenderImpl::CastSenderImpl(
    scoped_refptr<CastEnvironment> cast_environment,
    const AudioSenderConfig* audio_config,
    const VideoSenderConfig* video_config,
    const scoped_refptr<GpuVideoAcceleratorFactories>& gpu_factories,
    const CastInitializationCallback& initialization_status,
    transport::CastTransportSender* const transport_sender)
    : initialization_callback_(initialization_status),
      packet_receiver_(
          base::Bind(&CastSenderImpl::ReceivedPacket, base::Unretained(this))),
      cast_environment_(cast_environment),
      weak_factory_(this) {
  CHECK(cast_environment);
  CHECK(audio_config || video_config);

  base::WeakPtr<AudioSender> audio_sender_ptr;
  base::WeakPtr<VideoSender> video_sender_ptr;

  if (audio_config) {
    CHECK(audio_config->use_external_encoder ||
          cast_environment->HasAudioEncoderThread());

    audio_sender_.reset(
        new AudioSender(cast_environment, *audio_config, transport_sender));
    ssrc_of_audio_sender_ = audio_config->incoming_feedback_ssrc;
    audio_sender_ptr = audio_sender_->AsWeakPtr();

    CastInitializationStatus status = audio_sender_->InitializationResult();
    if (status != STATUS_INITIALIZED || !video_config) {
      if (status == STATUS_INITIALIZED && !video_config) {
        // Audio only.
        frame_input_ = new LocalFrameInput(
            cast_environment, audio_sender_ptr, video_sender_ptr);
      }
      cast_environment->PostTask(
          CastEnvironment::MAIN,
          FROM_HERE,
          base::Bind(&CastSenderImpl::InitializationResult,
                     weak_factory_.GetWeakPtr(),
                     status));
      return;
    }
  }
  if (video_config) {
    CHECK(video_config->use_external_encoder ||
          cast_environment->HasVideoEncoderThread());

    video_sender_.reset(
        new VideoSender(cast_environment,
                        *video_config,
                        gpu_factories,
                        base::Bind(&CastSenderImpl::InitializationResult,
                                   weak_factory_.GetWeakPtr()),
                        transport_sender));
    video_sender_ptr = video_sender_->AsWeakPtr();
    ssrc_of_video_sender_ = video_config->incoming_feedback_ssrc;
  }
  frame_input_ =
      new LocalFrameInput(cast_environment, audio_sender_ptr, video_sender_ptr);

  // Handing over responsibility to call NotifyInitialization to the
  // video sender.
}

CastSenderImpl::~CastSenderImpl() {}

// ReceivedPacket handle the incoming packets to the cast sender
// it's only expected to receive RTCP feedback packets from the remote cast
// receiver. The class verifies that that it is a RTCP packet and based on the
// SSRC of the incoming packet route the packet to the correct sender; audio or
// video.
//
// Definition of SSRC as defined in RFC 3550.
// Synchronization source (SSRC): The source of a stream of RTP
//    packets, identified by a 32-bit numeric SSRC identifier carried in
//    the RTP header so as not to be dependent upon the network address.
//    All packets from a synchronization source form part of the same
//    timing and sequence number space, so a receiver groups packets by
//    synchronization source for playback.  Examples of synchronization
//    sources include the sender of a stream of packets derived from a
//    signal source such as a microphone or a camera, or an RTP mixer
//    (see below).  A synchronization source may change its data format,
//    e.g., audio encoding, over time.  The SSRC identifier is a
//    randomly chosen value meant to be globally unique within a
//    particular RTP session (see Section 8).  A participant need not
//    use the same SSRC identifier for all the RTP sessions in a
//    multimedia session; the binding of the SSRC identifiers is
//    provided through RTCP (see Section 6.5.1).  If a participant
//    generates multiple streams in one RTP session, for example from
//    separate video cameras, each MUST be identified as a different
//    SSRC.
void CastSenderImpl::ReceivedPacket(scoped_ptr<Packet> packet) {
  DCHECK(cast_environment_);
  size_t length = packet->size();
  const uint8_t* data = &packet->front();
  if (!Rtcp::IsRtcpPacket(data, length)) {
    // We should have no incoming RTP packets.
    VLOG(1) << "Unexpectedly received a RTP packet in the cast sender";
    return;
  }
  uint32 ssrc_of_sender = Rtcp::GetSsrcOfSender(data, length);
  if (ssrc_of_sender == ssrc_of_audio_sender_) {
    if (!audio_sender_) {
      NOTREACHED();
      return;
    }
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&AudioSender::IncomingRtcpPacket,
                                           audio_sender_->AsWeakPtr(),
                                           base::Passed(&packet)));
  } else if (ssrc_of_sender == ssrc_of_video_sender_) {
    if (!video_sender_) {
      NOTREACHED();
      return;
    }
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&VideoSender::IncomingRtcpPacket,
                                           video_sender_->AsWeakPtr(),
                                           base::Passed(&packet)));
  } else {
    VLOG(1) << "Received a RTCP packet with a non matching sender SSRC "
            << ssrc_of_sender;
  }
}

scoped_refptr<FrameInput> CastSenderImpl::frame_input() { return frame_input_; }

transport::PacketReceiverCallback CastSenderImpl::packet_receiver() {
  return packet_receiver_;
  return base::Bind(&CastSenderImpl::ReceivedPacket, base::Unretained(this));
}

void CastSenderImpl::InitializationResult(CastInitializationStatus status)
    const {
  initialization_callback_.Run(status);
}

}  // namespace cast
}  // namespace media
