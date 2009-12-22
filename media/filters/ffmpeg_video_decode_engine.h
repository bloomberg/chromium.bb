// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODE_ENGINE_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODE_ENGINE_H_

#include "media/filters/video_decode_engine.h"

// FFmpeg types.
struct AVCodecContext;
struct AVFrame;
struct AVStream;

namespace media {

class InputBuffer;
class OmxCodec;

class FFmpegVideoDecodeEngine : public VideoDecodeEngine {
 public:
  FFmpegVideoDecodeEngine();
  virtual ~FFmpegVideoDecodeEngine();

  // Implementation of the VideoDecodeEngine Interface.
  virtual void Initialize(AVStream* stream, Task* done_cb);
  virtual void DecodeFrame(const Buffer& buffer, AVFrame* yuv_frame,
                           bool* got_result, Task* done_cb);
  virtual void Flush(Task* done_cb);
  virtual VideoSurface::Format GetSurfaceFormat() const;

  virtual State state() const { return state_; }

  virtual AVCodecContext* codec_context() const { return codec_context_; }

  virtual void SetCodecContextForTest(AVCodecContext* context) {
    codec_context_ = context;
  }

 private:
  AVCodecContext* codec_context_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecodeEngine);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODE_ENGINE_H_
