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
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/video_sender/video_sender.h"

namespace media {
  class VideoFrame;
}

namespace media {
namespace cast {

class AudioSender;
class PacedSender;
class VideoSender;

// This calls is a pure owner class that group all required sending objects
// together such as pacer, packet receiver, frame input, audio and video sender.
class CastSenderImpl : public CastSender {
 public:
  CastSenderImpl(scoped_refptr<CastEnvironment> cast_environment,
                 const AudioSenderConfig& audio_config,
                 const VideoSenderConfig& video_config,
                 VideoEncoderController* const video_encoder_controller,
                 PacketSender* const packet_sender);

  virtual ~CastSenderImpl();

  virtual scoped_refptr<FrameInput> frame_input() OVERRIDE;
  virtual scoped_refptr<PacketReceiver> packet_receiver() OVERRIDE;

 private:
  PacedSender pacer_;
  AudioSender audio_sender_;
  VideoSender video_sender_;
  scoped_refptr<FrameInput> frame_input_;
  scoped_refptr<PacketReceiver> packet_receiver_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_SENDER_IMPL_H_

