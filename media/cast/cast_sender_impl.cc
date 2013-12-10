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
    cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
        base::Bind(&VideoSender::InsertRawVideoFrame, video_sender_,
            video_frame, capture_time));
  }

  virtual void InsertCodedVideoFrame(const EncodedVideoFrame* video_frame,
                                     const base::TimeTicks& capture_time,
                                     const base::Closure callback) OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
        base::Bind(&VideoSender::InsertCodedVideoFrame, video_sender_,
            video_frame, capture_time, callback));
  }

  virtual void InsertAudio(const AudioBus* audio_bus,
                           const base::TimeTicks& recorded_time,
                           const base::Closure& done_callback) OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
        base::Bind(&AudioSender::InsertAudio, audio_sender_,
            audio_bus, recorded_time, done_callback));
  }

  virtual void InsertCodedAudioFrame(const EncodedAudioFrame* audio_frame,
                                     const base::TimeTicks& recorded_time,
                                     const base::Closure callback) OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
        base::Bind(&AudioSender::InsertCodedAudioFrame, audio_sender_,
            audio_frame, recorded_time, callback));
  }

 protected:
  virtual ~LocalFrameInput() {}

 private:
  friend class base::RefCountedThreadSafe<LocalFrameInput>;

  scoped_refptr<CastEnvironment> cast_environment_;
  base::WeakPtr<AudioSender> audio_sender_;
  base::WeakPtr<VideoSender> video_sender_;
};

// LocalCastSenderPacketReceiver handle the incoming packets to the cast sender
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

class LocalCastSenderPacketReceiver : public PacketReceiver {
 public:
  LocalCastSenderPacketReceiver(scoped_refptr<CastEnvironment> cast_environment,
                                base::WeakPtr<AudioSender> audio_sender,
                                base::WeakPtr<VideoSender> video_sender,
                                uint32 ssrc_of_audio_sender,
                                uint32 ssrc_of_video_sender)
      : cast_environment_(cast_environment),
        audio_sender_(audio_sender),
        video_sender_(video_sender),
        ssrc_of_audio_sender_(ssrc_of_audio_sender),
        ssrc_of_video_sender_(ssrc_of_video_sender) {}

  virtual void ReceivedPacket(const uint8* packet,
                              size_t length,
                              const base::Closure callback) OVERRIDE {
    if (!Rtcp::IsRtcpPacket(packet, length)) {
      // We should have no incoming RTP packets.
      // No action; just log and call the callback informing that we are done
      // with the packet.
      VLOG(1) << "Unexpectedly received a RTP packet in the cast sender";
      cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE, callback);
      return;
    }
    uint32 ssrc_of_sender = Rtcp::GetSsrcOfSender(packet, length);
    if (ssrc_of_sender == ssrc_of_audio_sender_) {
      cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
          base::Bind(&AudioSender::IncomingRtcpPacket, audio_sender_,
              packet, length, callback));
    } else if (ssrc_of_sender == ssrc_of_video_sender_) {
      cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
          base::Bind(&VideoSender::IncomingRtcpPacket, video_sender_,
              packet, length, callback));
    } else {
      // No action; just log and call the callback informing that we are done
      // with the packet.
      VLOG(1) << "Received a RTCP packet with a non matching sender SSRC "
              << ssrc_of_sender;

      cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE, callback);
    }
  }

 protected:
  virtual ~LocalCastSenderPacketReceiver() {}

 private:
  friend class base::RefCountedThreadSafe<LocalCastSenderPacketReceiver>;

  scoped_refptr<CastEnvironment> cast_environment_;
  base::WeakPtr<AudioSender> audio_sender_;
  base::WeakPtr<VideoSender> video_sender_;
  const uint32 ssrc_of_audio_sender_;
  const uint32 ssrc_of_video_sender_;
};

CastSender* CastSender::CreateCastSender(
    scoped_refptr<CastEnvironment> cast_environment,
    const AudioSenderConfig& audio_config,
    const VideoSenderConfig& video_config,
    VideoEncoderController* const video_encoder_controller,
    PacketSender* const packet_sender) {
  return new CastSenderImpl(cast_environment,
                            audio_config,
                            video_config,
                            video_encoder_controller,
                            packet_sender);
}

CastSenderImpl::CastSenderImpl(
    scoped_refptr<CastEnvironment> cast_environment,
    const AudioSenderConfig& audio_config,
    const VideoSenderConfig& video_config,
    VideoEncoderController* const video_encoder_controller,
    PacketSender* const packet_sender)
    : pacer_(cast_environment, packet_sender),
      audio_sender_(cast_environment, audio_config, &pacer_),
      video_sender_(cast_environment, video_config, video_encoder_controller,
                    &pacer_),
      frame_input_(new LocalFrameInput(cast_environment,
                                       audio_sender_.AsWeakPtr(),
                                       video_sender_.AsWeakPtr())),
      packet_receiver_(new LocalCastSenderPacketReceiver(cast_environment,
          audio_sender_.AsWeakPtr(), video_sender_.AsWeakPtr(),
          audio_config.incoming_feedback_ssrc,
          video_config.incoming_feedback_ssrc)) {}

CastSenderImpl::~CastSenderImpl() {}

scoped_refptr<FrameInput> CastSenderImpl::frame_input() {
  return frame_input_;
}

scoped_refptr<PacketReceiver> CastSenderImpl::packet_receiver() {
  return packet_receiver_;
}

}  // namespace cast
}  // namespace media
