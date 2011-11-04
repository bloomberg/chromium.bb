// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_FFMPEG_VIDEO_DECODE_ENGINE_H_
#define MEDIA_VIDEO_FFMPEG_VIDEO_DECODE_ENGINE_H_

#include "base/compiler_specific.h"
#include "media/video/video_decode_engine.h"

struct AVCodecContext;
struct AVFrame;

namespace media {

class MEDIA_EXPORT FFmpegVideoDecodeEngine : public VideoDecodeEngine {
 public:
  FFmpegVideoDecodeEngine();
  virtual ~FFmpegVideoDecodeEngine();

  // VideoDecodeEngine implementation.
  virtual bool Initialize(const VideoDecoderConfig& config) OVERRIDE;
  virtual void Uninitialize() OVERRIDE;
  virtual bool Decode(const scoped_refptr<Buffer>& buffer,
                      scoped_refptr<VideoFrame>* video_frame) OVERRIDE;
  virtual void Flush() OVERRIDE;

 private:
  // Allocates a video frame based on the current format and dimensions based on
  // the current state of |codec_context_|.
  scoped_refptr<VideoFrame> AllocateVideoFrame();

  // FFmpeg structures owned by this object.
  AVCodecContext* codec_context_;
  AVFrame* av_frame_;

  // Frame rate of the video.
  int frame_rate_numerator_;
  int frame_rate_denominator_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecodeEngine);
};

}  // namespace media

#endif  // MEDIA_VIDEO_FFMPEG_VIDEO_DECODE_ENGINE_H_
