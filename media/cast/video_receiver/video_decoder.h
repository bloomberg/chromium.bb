// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_VIDEO_RECEIVER_VIDEO_DECODER_H_
#define MEDIA_CAST_VIDEO_RECEIVER_VIDEO_DECODER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/cast_thread.h"

namespace media {
namespace cast {

class Vp8Decoder;

class VideoDecoder : public base::RefCountedThreadSafe<VideoDecoder>{
 public:
  VideoDecoder(scoped_refptr<CastThread> cast_thread,
               const VideoReceiverConfig& video_config);
  ~VideoDecoder();


  // Decode a video frame. Decoded (raw) frame will be returned in the
  // frame_decoded_callback.
  void DecodeVideoFrame(const EncodedVideoFrame* encoded_frame,
                        const base::TimeTicks render_time,
                        const VideoFrameDecodedCallback& frame_decoded_callback,
                        base::Closure frame_release_callback);

 private:
  void DecodeFrame(const EncodedVideoFrame* encoded_frame,
                   const base::TimeTicks render_time,
                   const VideoFrameDecodedCallback& frame_decoded_callback);
  VideoCodec codec_;
  scoped_ptr<Vp8Decoder> vp8_decoder_;
  scoped_refptr<CastThread> cast_thread_;

  DISALLOW_COPY_AND_ASSIGN(VideoDecoder);
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_VIDEO_RECEIVER_VIDEO_DECODER_H_
