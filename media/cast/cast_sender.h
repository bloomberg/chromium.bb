// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_SENDER_H_
#define MEDIA_CAST_CAST_SENDER_H_

#include "base/basictypes.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"

namespace media {
namespace cast {

class FrameInput {
 public:
  virtual void InsertRawVideoFrame(const I420VideoFrame& video_frame,
                                   base::TimeTicks capture_time) = 0;

  virtual void InsertCodedVideoFrame(const EncodedVideoFrame& video_frame,
                                     base::TimeTicks capture_time) = 0;

  virtual void InsertRawAudioFrame(const PcmAudioFrame& audio_frame,
                                   base::TimeTicks recorded_time) = 0;

  virtual void InsertCodedAudioFrame(const EncodedAudioFrame& audio_frame,
                                     base::TimeTicks recorded_time) = 0;

 protected:
  virtual ~FrameInput() {}
};

class CastSender {
 public:
  static CastSender* CreateCastSender(
      const AudioSenderConfig& audio_config,
      const VideoSenderConfig& video_config,
      VideoEncoderController* const video_encoder_controller,
      PacketSender* const packet_sender);

  virtual ~CastSender() {};

  virtual FrameInput* frame_input() = 0;

  // All RTCP packets for the call should be inserted to this
  // PacketReceiver. The PacketReceiver pointer is valid as long as the
  // CastSender instance exists.
  virtual PacketReceiver* packet_receiver() = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_SENDER_H_
