// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/cast_receiver_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"

namespace media {
namespace cast {

// The video and audio receivers should only be called from the main thread.
// LocalFrameReciever posts tasks to the main thread, making the cast interface
// thread safe.
class LocalFrameReceiver : public FrameReceiver {
 public:
  LocalFrameReceiver(scoped_refptr<CastEnvironment> cast_environment,
                     AudioReceiver* audio_receiver,
                     VideoReceiver* video_receiver)
      : cast_environment_(cast_environment),
        audio_receiver_(audio_receiver),
        video_receiver_(video_receiver) {}

  virtual void GetRawVideoFrame(
      const VideoFrameDecodedCallback& callback) OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
        base::Bind(&VideoReceiver::GetRawVideoFrame,
                   video_receiver_->AsWeakPtr(), callback));
  }

  virtual void GetEncodedVideoFrame(
      const VideoFrameEncodedCallback& callback) OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
        base::Bind(&VideoReceiver::GetEncodedVideoFrame,
                   video_receiver_->AsWeakPtr(), callback));
  }

  virtual void GetRawAudioFrame(
      int number_of_10ms_blocks,
      int desired_frequency,
      const AudioFrameDecodedCallback& callback) OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE, base::Bind(
        &AudioReceiver::GetRawAudioFrame, audio_receiver_->AsWeakPtr(),
        number_of_10ms_blocks, desired_frequency, callback));
  }

  virtual void GetCodedAudioFrame(
      const AudioFrameEncodedCallback& callback) OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
        base::Bind(&AudioReceiver::GetEncodedAudioFrame,
                   audio_receiver_->AsWeakPtr(), callback));
  }

 protected:
  virtual ~LocalFrameReceiver() {}

 private:
  friend class base::RefCountedThreadSafe<LocalFrameReceiver>;

  scoped_refptr<CastEnvironment> cast_environment_;
  AudioReceiver* audio_receiver_;
  VideoReceiver* video_receiver_;
};

// The video and audio receivers should only be called from the main thread.
class LocalPacketReceiver : public PacketReceiver {
 public:
  LocalPacketReceiver(scoped_refptr<CastEnvironment> cast_environment,
                      AudioReceiver* audio_receiver,
                      VideoReceiver* video_receiver,
                      uint32 ssrc_of_audio_sender,
                      uint32 ssrc_of_video_sender)
      : cast_environment_(cast_environment),
        audio_receiver_(audio_receiver),
        video_receiver_(video_receiver),
        ssrc_of_audio_sender_(ssrc_of_audio_sender),
        ssrc_of_video_sender_(ssrc_of_video_sender) {}

  virtual void ReceivedPacket(const uint8* packet,
                              size_t length,
                              const base::Closure callback) OVERRIDE {
    if (length < kMinLengthOfRtcp) {
      // No action; just log and call the callback informing that we are done
      // with the packet.
      VLOG(1) << "Received a packet which is too short " << length;
      cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE, callback);
      return;
    }
    uint32 ssrc_of_sender;
    if (!Rtcp::IsRtcpPacket(packet, length)) {
      if (length < kMinLengthOfRtp) {
        // No action; just log and call the callback informing that we are done
        // with the packet.
        VLOG(1) << "Received a RTP packet which is too short " << length;
        cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE, callback);
        return;
      }
      ssrc_of_sender = RtpReceiver::GetSsrcOfSender(packet, length);
    } else {
      ssrc_of_sender = Rtcp::GetSsrcOfSender(packet, length);
    }
    if (ssrc_of_sender == ssrc_of_audio_sender_) {
      cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
          base::Bind(&AudioReceiver::IncomingPacket,
                     audio_receiver_->AsWeakPtr(), packet, length, callback));
    } else if (ssrc_of_sender == ssrc_of_video_sender_) {
      cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE,
          base::Bind(&VideoReceiver::IncomingPacket,
                     video_receiver_->AsWeakPtr(), packet, length, callback));
    } else {
      // No action; just log and call the callback informing that we are done
      // with the packet.
      VLOG(1) << "Received a packet with a non matching sender SSRC "
              << ssrc_of_sender;

      cast_environment_->PostTask(CastEnvironment::MAIN, FROM_HERE, callback);
    }
  }

 protected:
  virtual ~LocalPacketReceiver() {}

 private:
  friend class base::RefCountedThreadSafe<LocalPacketReceiver>;

  scoped_refptr<CastEnvironment> cast_environment_;
  AudioReceiver* audio_receiver_;
  VideoReceiver* video_receiver_;
  const uint32 ssrc_of_audio_sender_;
  const uint32 ssrc_of_video_sender_;
};

CastReceiver* CastReceiver::CreateCastReceiver(
    scoped_refptr<CastEnvironment> cast_environment,
    const AudioReceiverConfig& audio_config,
    const VideoReceiverConfig& video_config,
    PacketSender* const packet_sender) {
  return new CastReceiverImpl(cast_environment,
                              audio_config,
                              video_config,
                              packet_sender);
}

CastReceiverImpl::CastReceiverImpl(
    scoped_refptr<CastEnvironment> cast_environment,
    const AudioReceiverConfig& audio_config,
    const VideoReceiverConfig& video_config,
    PacketSender* const packet_sender)
    : pacer_(cast_environment, packet_sender),
      audio_receiver_(cast_environment, audio_config, &pacer_),
      video_receiver_(cast_environment, video_config, &pacer_),
      frame_receiver_(new LocalFrameReceiver(cast_environment,
                                             &audio_receiver_,
                                             &video_receiver_)),
      packet_receiver_(new LocalPacketReceiver(cast_environment,
                                               &audio_receiver_,
                                               &video_receiver_,
                                               audio_config.incoming_ssrc,
                                               video_config.incoming_ssrc)) {}

CastReceiverImpl::~CastReceiverImpl() {}

scoped_refptr<PacketReceiver> CastReceiverImpl::packet_receiver() {
  return packet_receiver_;
}

scoped_refptr<FrameReceiver> CastReceiverImpl::frame_receiver() {
  return frame_receiver_;
}

}  // namespace cast
}  // namespace media
