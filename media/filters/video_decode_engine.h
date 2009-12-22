// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef MEDIA_FILTERS_VIDEO_DECODE_ENGINE_H_
#define MEDIA_FILTERS_VIDEO_DECODE_ENGINE_H_

#include "media/base/buffers.h"  // For VideoSurface.

// FFmpeg types.
//
// TODO(ajwong): Try to cut the dependency on the FFmpeg types.
struct AVFrame;
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

  // Initialized the engine.  On successful Initialization, state() should
  // return kNormal.
  virtual void Initialize(AVStream* stream, Task* done_cb) = 0;

  // Decodes one frame of video with the given buffer.  Returns false if there
  // was a decode error, or a zero-byte frame was produced.
  //
  // TODO(ajwong): Should this function just allocate a new yuv_frame and return
  // it via a "GetNextFrame()" method or similar?
  virtual void DecodeFrame(const Buffer& buffer, AVFrame* yuv_frame,
                           bool* got_result, Task* done_cb) = 0;

  // Flushes the decode engine of any buffered input packets.
  virtual void Flush(Task* done_cb) = 0;

  // Returns the VideoSurface::Format of the resulting |yuv_frame| from
  // DecodeFrame().
  virtual VideoSurface::Format GetSurfaceFormat() const = 0;

  // Returns the current state of the decode engine.
  virtual State state() const  = 0;

};

}  // namespace media

#endif  // MEDIA_FILTERS_VIDEO_DECODE_ENGINE_H_
