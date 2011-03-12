// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_FFMPEG_VIDEO_ALLOCATOR_H_
#define MEDIA_VIDEO_FFMPEG_VIDEO_ALLOCATOR_H_

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"

#include <deque>
#include <map>

// FFmpeg types.
struct AVCodecContext;
struct AVFrame;
struct AVStream;

namespace media {

class FFmpegVideoAllocator {
 public:
  FFmpegVideoAllocator();
  virtual ~FFmpegVideoAllocator();

  struct RefCountedAVFrame {
    RefCountedAVFrame() : usage_count_(0) {}

    // TODO(jiesun): we had commented out "DCHECK_EQ(usage_count_, 0);" here.
    // Because the way FFMPEG-MT handle release buffer in delayed fashion.
    // Probably we could wait FFMPEG-MT release all buffers before we callback
    // the flush completion.
    ~RefCountedAVFrame() {}

    void AddRef() {
      base::AtomicRefCountIncN(&usage_count_, 1);
    }

    bool Release() {
      return base::AtomicRefCountDecN(&usage_count_, 1);
    }

    AVFrame av_frame_;
    base::AtomicRefCount usage_count_;
  };

  static int AllocateBuffer(AVCodecContext* codec_context, AVFrame* av_frame);
  static void ReleaseBuffer(AVCodecContext* codec_context, AVFrame* av_frame);

  void Initialize(AVCodecContext* codec_context,
                  VideoFrame::Format surface_format);
  void Stop(AVCodecContext* codec_context);

  // DisplayDone() is called when renderer has finished using a frame.
  void DisplayDone(AVCodecContext* codec_context,
                   scoped_refptr<VideoFrame> video_frame);

  // DecodeDone() is called after avcodec_video_decode() finish so that we can
  // acquire a reference to the video frame before we hand it to the renderer.
  scoped_refptr<VideoFrame> DecodeDone(AVCodecContext* codec_context,
                                       AVFrame* av_frame);

 private:
  int InternalAllocateBuffer(AVCodecContext* codec_context, AVFrame* av_frame);
  void InternalReleaseBuffer(AVCodecContext* codec_context, AVFrame* av_frame);

  VideoFrame::Format surface_format_;

  // This queue keeps reference count for all VideoFrame allocated.
  std::deque<RefCountedAVFrame*> frame_pool_;

  // This queue keeps per-AVCodecContext VideoFrame allocation that
  // was available for recycling.
  static const int kMaxFFmpegThreads = 3;
  std::deque<RefCountedAVFrame*> available_frames_[kMaxFFmpegThreads];

  // This map is used to map from AVCodecContext* to index to
  // |available_frames_|, because ffmpeg-mt maintain multiple
  // AVCodecContext (per thread).
  std::map<void*, int> codec_index_map_;

  // These function pointer store original ffmpeg AVCodecContext's
  // get_buffer()/release_buffer() function pointer. We use these function
  // to delegate the allocation request.
  int (*get_buffer_)(struct AVCodecContext *c, AVFrame *pic);
  void (*release_buffer_)(struct AVCodecContext *c, AVFrame *pic);
};

} // namespace media

#endif  // MEDIA_VIDEO_FFMPEG_VIDEO_ALLOCATOR_H_
