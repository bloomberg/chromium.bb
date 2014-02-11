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
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"

namespace media {
class VideoFrame;

namespace cast {

namespace transport {
class PacketSender;
}

// Callback in which the raw audio frame and play-out time will be returned
// once decoding is complete.
typedef base::Callback<void(scoped_ptr<PcmAudioFrame>, const base::TimeTicks&)>
    AudioFrameDecodedCallback;

// Callback in which the encoded audio frame and play-out time will be returned.
typedef base::Callback<void(scoped_ptr<transport::EncodedAudioFrame>,
                            const base::TimeTicks&)> AudioFrameEncodedCallback;

// Callback in which the raw frame and render time will be returned once
// decoding is complete.
typedef base::Callback<void(const scoped_refptr<media::VideoFrame>& video_frame,
                            const base::TimeTicks&)> VideoFrameDecodedCallback;

// Callback in which the encoded video frame and render time will be returned.
typedef base::Callback<void(scoped_ptr<transport::EncodedVideoFrame>,
                            const base::TimeTicks&)> VideoFrameEncodedCallback;

// This Class is thread safe.
class FrameReceiver : public base::RefCountedThreadSafe<FrameReceiver> {
 public:
  virtual void GetRawAudioFrame(int number_of_10ms_blocks,
                                int desired_frequency,
                                const AudioFrameDecodedCallback& callback) = 0;

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
  static CastReceiver* CreateCastReceiver(
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
