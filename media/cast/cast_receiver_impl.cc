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

  virtual void GetRawVideoFrame(const VideoFrameDecodedCallback& callback)
      OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&VideoReceiver::GetRawVideoFrame,
                                           video_receiver_->AsWeakPtr(),
                                           callback));
  }

  virtual void GetEncodedVideoFrame(const VideoFrameEncodedCallback& callback)
      OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&VideoReceiver::GetEncodedVideoFrame,
                                           video_receiver_->AsWeakPtr(),
                                           callback));
  }

  virtual void GetRawAudioFrame(const AudioFrameDecodedCallback& callback)
      OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&AudioReceiver::GetRawAudioFrame,
                                           audio_receiver_->AsWeakPtr(),
                                           callback));
  }

  virtual void GetCodedAudioFrame(const AudioFrameEncodedCallback& callback)
      OVERRIDE {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&AudioReceiver::GetEncodedAudioFrame,
                                           audio_receiver_->AsWeakPtr(),
                                           callback));
  }

 protected:
  virtual ~LocalFrameReceiver() {}

 private:
  friend class base::RefCountedThreadSafe<LocalFrameReceiver>;

  scoped_refptr<CastEnvironment> cast_environment_;
  AudioReceiver* audio_receiver_;
  VideoReceiver* video_receiver_;
};

scoped_ptr<CastReceiver> CastReceiver::Create(
    scoped_refptr<CastEnvironment> cast_environment,
    const AudioReceiverConfig& audio_config,
    const VideoReceiverConfig& video_config,
    transport::PacketSender* const packet_sender) {
  return scoped_ptr<CastReceiver>(new CastReceiverImpl(
      cast_environment, audio_config, video_config, packet_sender));
}

CastReceiverImpl::CastReceiverImpl(
    scoped_refptr<CastEnvironment> cast_environment,
    const AudioReceiverConfig& audio_config,
    const VideoReceiverConfig& video_config,
    transport::PacketSender* const packet_sender)
    : pacer_(cast_environment->Clock(),
             cast_environment->Logging(),
             packet_sender,
             cast_environment->GetTaskRunner(CastEnvironment::MAIN)),
      audio_receiver_(cast_environment, audio_config, &pacer_),
      video_receiver_(cast_environment,
                      video_config,
                      &pacer_),
      frame_receiver_(new LocalFrameReceiver(cast_environment,
                                             &audio_receiver_,
                                             &video_receiver_)),
      cast_environment_(cast_environment),
      ssrc_of_audio_sender_(audio_config.incoming_ssrc),
      ssrc_of_video_sender_(video_config.incoming_ssrc) {}

CastReceiverImpl::~CastReceiverImpl() {}

// The video and audio receivers should only be called from the main thread.
void CastReceiverImpl::ReceivedPacket(scoped_ptr<Packet> packet) {
  const uint8_t* data = &packet->front();
  size_t length = packet->size();
  if (length < kMinLengthOfRtcp) {
    VLOG(1) << "Received a packet which is too short " << length;
    return;
  }
  uint32 ssrc_of_sender;
  if (!Rtcp::IsRtcpPacket(data, length)) {
    if (length < kMinLengthOfRtp) {
      VLOG(1) << "Received a RTP packet which is too short " << length;
      return;
    }
    ssrc_of_sender = RtpReceiver::GetSsrcOfSender(data, length);
  } else {
    ssrc_of_sender = Rtcp::GetSsrcOfSender(data, length);
  }
  if (ssrc_of_sender == ssrc_of_audio_sender_) {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&AudioReceiver::IncomingPacket,
                                           audio_receiver_.AsWeakPtr(),
                                           base::Passed(&packet)));
  } else if (ssrc_of_sender == ssrc_of_video_sender_) {
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&VideoReceiver::IncomingPacket,
                                           video_receiver_.AsWeakPtr(),
                                           base::Passed(&packet)));
  } else {
    VLOG(1) << "Received a packet with a non matching sender SSRC "
            << ssrc_of_sender;
  }
}

transport::PacketReceiverCallback CastReceiverImpl::packet_receiver() {
  return base::Bind(&CastReceiverImpl::ReceivedPacket, base::Unretained(this));
}

scoped_refptr<FrameReceiver> CastReceiverImpl::frame_receiver() {
  return frame_receiver_;
}

}  // namespace cast
}  // namespace media
