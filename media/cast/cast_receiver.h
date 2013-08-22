// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_CAST_RECEIVER_H_
#define MEDIA_CAST_CAST_RECEIVER_H_

#include "base/basictypes.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"

namespace media {
namespace cast {

class FrameReceiver {
 public:
  virtual bool GetRawVideoFrame(I420VideoFrame* video_frame,
                                base::TimeTicks* render_time) = 0;

  virtual bool GetEncodedVideoFrame(EncodedVideoFrame* video_frame,
                                    base::TimeTicks* render_time) = 0;

  virtual void ReleaseEncodedVideoFrame(uint8 frame_id) = 0;

  virtual bool GetRawAudioFrame(int number_of_10ms_blocks,
                                int desired_frequency,
                                PcmAudioFrame* audio_frame,
                                base::TimeTicks* playout_time) = 0;

  virtual bool GetCodedAudioFrame(EncodedAudioFrame* audio_frame,
                                  base::TimeTicks* playout_time) = 0;

  virtual void ReleaseCodedAudioFrame(uint8 frame_id) = 0;

protected:
  virtual ~FrameReceiver() {}
};

class CastReceiver {
 public:
  static CastReceiver* CreateCastReceiver(
      const AudioReceiverConfig& audio_config,
      const VideoReceiverConfig& video_config,
      PacketSender* const packet_sender);

  // All received RTP and RTCP packets for the call should be inserted to this
  // PacketReceiver. The PacketReceiver pointer is valid as long as the
  // CastReceiver instance exists.
  virtual PacketReceiver* packet_receiver() = 0;

  // Polling interface to get audio and video frames from the CastReceiver.
  virtual FrameReceiver* frame_receiver() = 0;

  virtual ~CastReceiver() {};
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_RECEIVER_H_
