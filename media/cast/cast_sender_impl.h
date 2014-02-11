// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_CAST_CAST_SENDER_IMPL_H_
#define MEDIA_CAST_CAST_SENDER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/cast/audio_sender/audio_sender.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_sender.h"
#include "media/cast/video_sender/video_sender.h"

namespace media {
class VideoFrame;
}

namespace media {
namespace cast {

class AudioSender;
class VideoSender;

// This calls is a pure owner class that group all required sending objects
// together such as pacer, packet receiver, frame input, audio and video sender.
class CastSenderImpl : public CastSender {
 public:
  CastSenderImpl(
      scoped_refptr<CastEnvironment> cast_environment,
      const AudioSenderConfig* audio_config,
      const VideoSenderConfig* video_config,
      const scoped_refptr<GpuVideoAcceleratorFactories>& gpu_factories,
      const CastInitializationCallback& initialization_status,
      transport::CastTransportSender* const transport_sender);

  virtual ~CastSenderImpl();

  virtual scoped_refptr<FrameInput> frame_input() OVERRIDE;
  virtual transport::PacketReceiverCallback packet_receiver() OVERRIDE;

 private:
  void ReceivedPacket(scoped_ptr<Packet> packet);
  // Used to trampoline the result back on the correct thread. And guaranteed
  // not to be called until the creation is complete.
  void InitializationResult(CastInitializationStatus status) const;

  CastInitializationCallback initialization_callback_;
  scoped_ptr<AudioSender> audio_sender_;
  scoped_ptr<VideoSender> video_sender_;
  scoped_refptr<FrameInput> frame_input_;
  transport::PacketReceiverCallback packet_receiver_;
  scoped_refptr<CastEnvironment> cast_environment_;
  uint32 ssrc_of_audio_sender_;
  uint32 ssrc_of_video_sender_;
  base::WeakPtrFactory<CastSenderImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastSenderImpl);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_SENDER_IMPL_H_
