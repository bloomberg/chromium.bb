// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_VIDEO_DECODE_ENGINE_H_
#define MEDIA_FILTERS_FFMPEG_VIDEO_DECODE_ENGINE_H_

#include "base/scoped_ptr.h"
#include "media/ffmpeg/ffmpeg_common.h"
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
  virtual void Initialize(MessageLoop* message_loop,
                          AVStream* av_stream,
                          EmptyThisBufferCallback* empty_buffer_callback,
                          FillThisBufferCallback* fill_buffer_callback,
                          Task* done_cb);
  virtual void EmptyThisBuffer(scoped_refptr<Buffer> buffer);
  virtual void FillThisBuffer(scoped_refptr<VideoFrame> frame) {}
  virtual void Stop(Task* done_cb);
  virtual void Pause(Task* done_cb);
  virtual void Flush(Task* done_cb);
  virtual VideoFrame::Format GetSurfaceFormat() const;

  virtual State state() const { return state_; }

  virtual AVCodecContext* codec_context() const { return codec_context_; }

  virtual void SetCodecContextForTest(AVCodecContext* context) {
    codec_context_ = context;
  }

 private:
  void DecodeFrame(scoped_refptr<Buffer> buffer);

  AVCodecContext* codec_context_;
  AVStream* av_stream_;
  State state_;
  scoped_ptr_malloc<AVFrame, ScopedPtrAVFree> av_frame_;
  scoped_ptr<FillThisBufferCallback> fill_this_buffer_callback_;
  scoped_ptr<EmptyThisBufferCallback> empty_this_buffer_callback_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecodeEngine);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_VIDEO_DECODE_ENGINE_H_
