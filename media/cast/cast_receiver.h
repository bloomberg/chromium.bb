// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is the main interface for the cast receiver. All configuration are done
// at creation.

#ifndef MEDIA_CAST_CAST_RECEIVER_H_
#define MEDIA_CAST_CAST_RECEIVER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "media/base/audio_bus.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"

namespace media {
class VideoFrame;

namespace cast {

namespace transport {
class PacketSender;
}

// The following callbacks are used to deliver decoded audio/video frame data,
// the frame's corresponding play-out time, and a continuity flag.
// |is_continuous| will be false to indicate the loss of data due to a loss of
// frames (or decoding errors).  This allows the client to take steps to smooth
// discontinuities for playback.  Note: A NULL pointer can be returned when data
// is not available (e.g., bad packet or when flushing callbacks during
// shutdown).
typedef base::Callback<void(scoped_ptr<AudioBus> audio_bus,
                            const base::TimeTicks& playout_time,
                            bool is_continuous)> AudioFrameDecodedCallback;
// TODO(miu): |video_frame| includes a timestamp, so use that instead.
typedef base::Callback<void(const scoped_refptr<media::VideoFrame>& video_frame,
                            const base::TimeTicks& playout_time,
                            bool is_continuous)> VideoFrameDecodedCallback;

// The following callbacks deliver still-encoded audio/video frame data, along
// with the frame's corresponding play-out time.  The client should examine the
// EncodedXXXFrame::frame_id field to determine whether any frames have been
// dropped (i.e., frame_id should be incrementing by one each time).  Note: A
// NULL pointer can be returned on error/shutdown.
typedef base::Callback<void(scoped_ptr<transport::EncodedAudioFrame>,
                            const base::TimeTicks&)> AudioFrameEncodedCallback;
typedef base::Callback<void(scoped_ptr<transport::EncodedVideoFrame>,
                            const base::TimeTicks&)> VideoFrameEncodedCallback;

// This Class is thread safe.
class FrameReceiver : public base::RefCountedThreadSafe<FrameReceiver> {
 public:
  virtual void GetRawAudioFrame(const AudioFrameDecodedCallback& callback) = 0;

  virtual void GetCodedAudioFrame(
      const AudioFrameEncodedCallback& callback) = 0;

  virtual void GetRawVideoFrame(const VideoFrameDecodedCallback& callback) = 0;

  virtual void GetEncodedVideoFrame(
      const VideoFrameEncodedCallback& callback) = 0;

 protected:
  virtual ~FrameReceiver() {}

 private:
  friend class base::RefCountedThreadSafe<FrameReceiver>;
};

// This Class is thread safe.
class CastReceiver {
 public:
  static scoped_ptr<CastReceiver> Create(
      scoped_refptr<CastEnvironment> cast_environment,
      const AudioReceiverConfig& audio_config,
      const VideoReceiverConfig& video_config,
      transport::PacketSender* const packet_sender);

  // All received RTP and RTCP packets for the call should be sent to this
  // PacketReceiver. Can be called from any function.
  // TODO(hubbe): Replace with:
  //   virtual void ReceivePacket(scoped_ptr<Packet> packet) = 0;
  virtual transport::PacketReceiverCallback packet_receiver() = 0;

  // Polling interface to get audio and video frames from the CastReceiver.
  virtual scoped_refptr<FrameReceiver> frame_receiver() = 0;

  virtual ~CastReceiver() {}
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_CAST_RECEIVER_H_
