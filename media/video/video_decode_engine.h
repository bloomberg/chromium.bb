// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_DECODE_ENGINE_H_
#define MEDIA_VIDEO_VIDEO_DECODE_ENGINE_H_

#include "base/callback.h"
#include "base/message_loop.h"
#include "media/base/video_frame.h"

namespace media {

class Buffer;

enum VideoCodec {
  kCodecH264,
  kCodecVC1,
  kCodecMPEG2,
  kCodecMPEG4,
  kCodecTheora,
  kCodecVP8,
};

static const uint32 kProfileDoNotCare = static_cast<uint32>(-1);
static const uint32 kLevelDoNotCare = static_cast<uint32>(-1);

struct VideoCodecConfig {
  VideoCodecConfig() : codec_(kCodecH264),
                       profile_(kProfileDoNotCare),
                       level_(kLevelDoNotCare),
                       width_(0),
                       height_(0),
                       opaque_context_(NULL) {}

  VideoCodec codec_;

  // TODO(jiesun): video profile and level are specific to individual codec.
  // Define enum to.
  uint32 profile_;
  uint32 level_;

  // Container's concept of width and height of this video.
  int32 width_;
  int32 height_;  // TODO(jiesun): Do we allow height to be negative to
                  // indicate output is upside-down?

  // FFMPEG's will use this to pass AVStream. Otherwise, we should remove this.
  void* opaque_context_;
};

struct VideoStreamInfo {
  VideoFrame::Format surface_format_;
  VideoFrame::SurfaceType surface_type_;
  uint32 surface_width_;  // Can be different with container's value.
  uint32 surface_height_;  // Can be different with container's value.
};

struct VideoCodecInfo {
  // Other parameter is only meaningful when this is true.
  bool success_;

  // Whether decoder provides output buffer pool.
  bool provides_buffers_;

  // Initial Stream Info. Only part of them could be valid.
  // If they are not valid, Engine should update with OnFormatChange.
  VideoStreamInfo stream_info_;
};

class VideoDecodeEngine : public base::RefCountedThreadSafe<VideoDecodeEngine> {
 public:
  struct EventHandler {
   public:
    virtual ~EventHandler() {}
    virtual void OnInitializeComplete(const VideoCodecInfo& info) = 0;
    virtual void OnUninitializeComplete() = 0;
    virtual void OnFlushComplete() = 0;
    virtual void OnSeekComplete() = 0;
    virtual void OnError() = 0;
    virtual void OnFormatChange(VideoStreamInfo stream_info) = 0;
    virtual void OnEmptyBufferCallback(scoped_refptr<Buffer> buffer) = 0;
    virtual void OnFillBufferCallback(scoped_refptr<VideoFrame> frame) = 0;
  };

  VideoDecodeEngine() {}
  virtual ~VideoDecodeEngine() {}

  // Initialized the engine with specified configuration. |message_loop| could
  // be NULL if every operation is synchronous. Engine should call the
  // EventHandler::OnInitializeDone() no matter finished successfully or not.
  // TODO(jiesun): remove message_loop and create thread inside openmax engine?
  // or create thread in GpuVideoDecoder and pass message loop here?
  virtual void Initialize(MessageLoop* message_loop,
                          EventHandler* event_handler,
                          const VideoCodecConfig& config) = 0;

  // Uninitialize the engine. Engine should destroy all resources and call
  // EventHandler::OnUninitializeComplete().
  virtual void Uninitialize() = 0;

  // Flush the engine. Engine should return all the buffers to owner ( which
  // could be itself. ) then call EventHandler::OnFlushDone().
  virtual void Flush() = 0;

  // Used in openmax to InitialReadBuffers().
  virtual void Seek() = 0;  // TODO(jiesun): Do we need this?

  // Buffer exchange method for input and output stream.
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
};

}  // namespace media

#endif  // MEDIA_VIDEO_VIDEO_DECODE_ENGINE_H_
