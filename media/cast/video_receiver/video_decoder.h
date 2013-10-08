// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_VIDEO_RECEIVER_VIDEO_DECODER_H_
#define MEDIA_CAST_VIDEO_RECEIVER_VIDEO_DECODER_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_receiver.h"

namespace media {
namespace cast {

class Vp8Decoder;

// This class is not thread safe; it's only called from the cast video decoder
// thread.
class VideoDecoder : public base::NonThreadSafe {
 public:
  explicit VideoDecoder(const VideoReceiverConfig& video_config);
  virtual ~VideoDecoder();

  // Decode a video frame. Decoded (raw) frame will be returned in the
  // provided video_frame.
  bool DecodeVideoFrame(const EncodedVideoFrame* encoded_frame,
                        const base::TimeTicks render_time,
                        I420VideoFrame* video_frame);

 private:
  VideoCodec codec_;
  scoped_ptr<Vp8Decoder> vp8_decoder_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_RECEIVER_VIDEO_DECODER_H_
