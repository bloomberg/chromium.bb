// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_DECODE_ENGINE_H_
#define MEDIA_FILTERS_VIDEO_DECODE_ENGINE_H_

#include "base/callback.h"
#include "base/message_loop.h"
#include "media/base/video_frame.h"

// FFmpeg types.
//
// TODO(ajwong): Try to cut the dependency on the FFmpeg types.
struct AVStream;

class Task;

namespace media {

class Buffer;

class VideoDecodeEngine {
 public:
  enum State {
    kCreated,
    kNormal,
    kStopped,
    kError,
  };

  VideoDecodeEngine() {}
  virtual ~VideoDecodeEngine() {}

  // This calback is called by the decode engine to notify that the decode
  // engine has consumed an input bufer in response to a EmptyThisBuffer() call.
  typedef Callback1<scoped_refptr<Buffer> >::Type EmptyThisBufferCallback;

  // This callback is called by the decode engine to notify that a video
  // frame is ready to be consumed in reaponse to a FillThisBuffer() call.
  typedef Callback1<scoped_refptr<VideoFrame> >::Type FillThisBufferCallback;

  // Initialized the engine.  On successful Initialization, state() should
  // return kNormal.
  virtual void Initialize(MessageLoop* message_loop,
                          AVStream* av_stream,
                          EmptyThisBufferCallback* empty_buffer_callback,
                          FillThisBufferCallback* fill_buffer_callback,
                          Task* done_cb) = 0;

  // These functions and callbacks could be used in two scenarios for both
  // input and output streams:
  // 1. Engine provide buffers.
  // 2. Outside party provide buffers.
  // The currently planned engine implementation:
  // 1. provides the input buffer request inside engine through
  // |EmptyThisBufferCallback|. The engine implementation has better knowledge
  // of the decoder reordering delay and jittery removal requirements. Input
  // buffers are returned into engine through |EmptyThisBuffer|.
  // 2. Output buffers are provided from outside the engine, and feed into
  // engine through |FillThisBuffer|. Output buffers are returned to outside
  // by |FillThisBufferCallback|.
  virtual void EmptyThisBuffer(scoped_refptr<Buffer> buffer) = 0;
  virtual void FillThisBuffer(scoped_refptr<VideoFrame> frame) = 0;

  virtual void Stop(Task* done_cb) = 0;
  virtual void Pause(Task* done_cb) = 0;

  // Flushes the decode engine of any buffered input packets.
  virtual void Flush(Task* done_cb) = 0;

  // Returns the VideoSurface::Format of the resulting |yuv_frame| from
  // DecodeFrame().
  virtual VideoFrame::Format GetSurfaceFormat() const = 0;

  // Returns the current state of the decode engine.
  virtual State state() const  = 0;
};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_DECODE_ENGINE_H_
