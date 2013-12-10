// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the main interface for the cast sender. All configuration are done
// at creation.
//
// The FrameInput and PacketReciever interfaces should normally be accessed from
// the IO thread. However they are allowed to be called from any thread.

#ifndef MEDIA_CAST_CAST_SENDER_H_
#define MEDIA_CAST_CAST_SENDER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"

namespace media {
class AudioBus;
class VideoFrame;
}

namespace media {
namespace cast {

// This Class is thread safe.
class FrameInput : public base::RefCountedThreadSafe<FrameInput> {
 public:
  // The video_frame must be valid until the callback is called.
  // The callback is called from the main cast thread as soon as
  // the encoder is done with the frame; it does not mean that the encoded frame
  // has been sent out.
  virtual void InsertRawVideoFrame(
      const scoped_refptr<media::VideoFrame>& video_frame,
      const base::TimeTicks& capture_time) = 0;

  // The video_frame must be valid until the callback is called.
  // The callback is called from the main cast thread as soon as
  // the cast sender is done with the frame; it does not mean that the encoded
  // frame has been sent out.
  virtual void InsertCodedVideoFrame(const EncodedVideoFrame* video_frame,
                                     const base::TimeTicks& capture_time,
                                     const base::Closure callback) = 0;

  // The |audio_bus| must be valid until the |done_callback| is called.
  // The callback is called from the main cast thread as soon as the encoder is
  // done with |audio_bus|; it does not mean that the encoded data has been
  // sent out.
  virtual void InsertAudio(const AudioBus* audio_bus,
                           const base::TimeTicks& recorded_time,
                           const base::Closure& done_callback) = 0;

  // The audio_frame must be valid until the callback is called.
  // The callback is called from the main cast thread as soon as
  // the cast sender is done with the frame; it does not mean that the encoded
  // frame has been sent out.
  virtual void InsertCodedAudioFrame(const EncodedAudioFrame* audio_frame,
                                     const base::TimeTicks& recorded_time,
                                     const base::Closure callback) = 0;

 protected:
  virtual ~FrameInput() {}

 private:
  friend class base::RefCountedThreadSafe<FrameInput>;
};

// This Class is thread safe.
// The provided PacketSender object will always be called form the main cast
// thread.
class CastSender {
 public:
  static CastSender* CreateCastSender(
      scoped_refptr<CastEnvironment> cast_environment,
      const AudioSenderConfig& audio_config,
      const VideoSenderConfig& video_config,
      VideoEncoderController* const video_encoder_controller,
      PacketSender* const packet_sender);

  virtual ~CastSender() {}

  // All audio and video frames for the session should be inserted to this
  // object.
  // Can be called from any thread.
  virtual scoped_refptr<FrameInput> frame_input() = 0;

  // All RTCP packets for the session should be inserted to this object.
  // Can be called from any thread.
  virtual scoped_refptr<PacketReceiver> packet_receiver() = 0;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_SENDER_H_
