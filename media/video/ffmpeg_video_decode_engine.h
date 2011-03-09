// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_FFMPEG_VIDEO_DECODE_ENGINE_H_
#define MEDIA_VIDEO_FFMPEG_VIDEO_DECODE_ENGINE_H_

#include <deque>

#include "base/scoped_ptr.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/video/video_decode_engine.h"

// FFmpeg types.
struct AVCodecContext;
struct AVFrame;

namespace media {

class FFmpegVideoAllocator;

class FFmpegVideoDecodeEngine : public VideoDecodeEngine {
 public:
  FFmpegVideoDecodeEngine();
  virtual ~FFmpegVideoDecodeEngine();

  // Implementation of the VideoDecodeEngine Interface.
  virtual void Initialize(MessageLoop* message_loop,
                          VideoDecodeEngine::EventHandler* event_handler,
                          VideoDecodeContext* context,
                          const VideoCodecConfig& config);
  virtual void ConsumeVideoSample(scoped_refptr<Buffer> buffer);
  virtual void ProduceVideoFrame(scoped_refptr<VideoFrame> frame);
  virtual void Uninitialize();
  virtual void Flush();
  virtual void Seek();

  VideoFrame::Format GetSurfaceFormat() const;

 private:
  void DecodeFrame(scoped_refptr<Buffer> buffer);
  void ReadInput();
  void TryToFinishPendingFlush();

  AVCodecContext* codec_context_;
  scoped_ptr_malloc<AVFrame, ScopedPtrAVFree> av_frame_;
  VideoDecodeEngine::EventHandler* event_handler_;

  // Frame rate of the video.
  int frame_rate_numerator_;
  int frame_rate_denominator_;

  // Whether direct rendering is used.
  bool direct_rendering_;

  // Used when direct rendering is used to recycle output buffers.
  scoped_ptr<FFmpegVideoAllocator> allocator_;

  // Indicate how many buffers are pending on input port of this filter:
  // Increment when engine receive one input packet from demuxer;
  // Decrement when engine send one input packet to demuxer;
  int pending_input_buffers_;

  // Indicate how many buffers are pending on output port of this filter:
  // Increment when engine receive one output frame from renderer;
  // Decrement when engine send one output frame to renderer;
  int pending_output_buffers_;

  // Whether end of stream had been reached at output side.
  bool output_eos_reached_;

  // Used when direct rendering is disabled to hold available output buffers.
  std::deque<scoped_refptr<VideoFrame> > frame_queue_available_;

  // Whether flush operation is pending.
  bool flush_pending_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecodeEngine);
};

}  // namespace media

#endif  // MEDIA_VIDEO_FFMPEG_VIDEO_DECODE_ENGINE_H_
