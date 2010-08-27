// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_video_allocator.h"

#include "base/logging.h"
#include "media/ffmpeg/ffmpeg_common.h"

// Because Chromium could be build with FFMPEG version other than FFMPEG-MT
// by using GYP_DEFINES variable "use-system-ffmpeg". The following code will
// not build with vanilla FFMPEG. We will fall back to "disable direct
// rendering" when that happens.
// TODO(jiesun): Actually we could do better than this: we should modify the
// following code to work with vanilla FFMPEG.

namespace media {

FFmpegVideoAllocator::FFmpegVideoAllocator()
    : get_buffer_(NULL),
      release_buffer_(NULL) {
}

void FFmpegVideoAllocator::Initialize(AVCodecContext* codec_context,
                                      VideoFrame::Format surface_format) {
#ifdef FF_THREAD_FRAME  // Only defined in FFMPEG-MT.
  surface_format_ = surface_format;
  get_buffer_ = codec_context->get_buffer;
  release_buffer_ = codec_context->release_buffer;
  codec_context->get_buffer = AllocateBuffer;
  codec_context->release_buffer = ReleaseBuffer;
  codec_context->opaque = this;
#endif
}

void FFmpegVideoAllocator::Stop(AVCodecContext* codec_context) {
#ifdef FF_THREAD_FRAME  // Only defined in FFMPEG-MT.
  // Restore default buffer allocator functions.
  // This does not work actually, because in ffmpeg-mt, there are
  // multiple codec_context copies per threads. each context maintain
  // its own internal buffer pools.
  codec_context->get_buffer = get_buffer_;
  codec_context->release_buffer = release_buffer_;

  while (!frame_pool_.empty()) {
    RefCountedAVFrame* ffmpeg_video_frame = frame_pool_.front();
    frame_pool_.pop_front();
    ffmpeg_video_frame->av_frame_.opaque = NULL;

    // Reset per-context default buffer release functions.
    ffmpeg_video_frame->av_frame_.owner->release_buffer = release_buffer_;
    ffmpeg_video_frame->av_frame_.owner->get_buffer = get_buffer_;
    delete ffmpeg_video_frame;
  }
  for (int i = 0; i < kMaxFFmpegThreads; ++i)
    available_frames_[i].clear();
  codec_index_map_.clear();
#endif
}

void FFmpegVideoAllocator::DisplayDone(
    AVCodecContext* codec_context,
    scoped_refptr<VideoFrame> video_frame) {
#ifdef FF_THREAD_FRAME  // Only defined in FFMPEG-MT.
  RefCountedAVFrame* ffmpeg_video_frame =
      reinterpret_cast<RefCountedAVFrame*>(video_frame->private_buffer());
  if (ffmpeg_video_frame->Release() == 0) {
    int index = codec_index_map_[ffmpeg_video_frame->av_frame_.owner];
    available_frames_[index].push_back(ffmpeg_video_frame);
  }
#endif
}

scoped_refptr<VideoFrame> FFmpegVideoAllocator::DecodeDone(
    AVCodecContext* codec_context,
    AVFrame* av_frame) {
  scoped_refptr<VideoFrame> frame;
#ifdef FF_THREAD_FRAME  // Only defined in FFMPEG-MT.
  RefCountedAVFrame* ffmpeg_video_frame =
      reinterpret_cast<RefCountedAVFrame*>(av_frame->opaque);
  ffmpeg_video_frame->av_frame_ = *av_frame;
  ffmpeg_video_frame->AddRef();

  VideoFrame::CreateFrameExternal(
      VideoFrame::TYPE_SYSTEM_MEMORY, surface_format_,
      codec_context->width, codec_context->height, 3,
      av_frame->data,
      av_frame->linesize,
      StreamSample::kInvalidTimestamp,
      StreamSample::kInvalidTimestamp,
      ffmpeg_video_frame,  // |private_buffer_|.
      &frame);
#endif
  return frame;
}

int FFmpegVideoAllocator::AllocateBuffer(AVCodecContext* codec_context,
                                            AVFrame* av_frame) {
  FFmpegVideoAllocator* context =
      reinterpret_cast<FFmpegVideoAllocator*>(codec_context->opaque);
  return context->InternalAllocateBuffer(codec_context, av_frame);
}

void FFmpegVideoAllocator::ReleaseBuffer(AVCodecContext* codec_context,
                                            AVFrame* av_frame) {
  FFmpegVideoAllocator* context =
      reinterpret_cast<FFmpegVideoAllocator*>(codec_context->opaque);
  context->InternalReleaseBuffer(codec_context, av_frame);
}

int FFmpegVideoAllocator::InternalAllocateBuffer(
    AVCodecContext* codec_context, AVFrame* av_frame) {
#ifdef FF_THREAD_FRAME  // Only defined in FFMPEG-MT.
  // If |codec_context| is not yet known to us, we add it to our map.
  if (codec_index_map_.find(codec_context) == codec_index_map_.end()) {
    int next_index = codec_index_map_.size();
    codec_index_map_[codec_context] = next_index;
    CHECK_LE((int)codec_index_map_.size(), kMaxFFmpegThreads);
  }

  int index = codec_index_map_[codec_context];

  RefCountedAVFrame* ffmpeg_video_frame;
  if (available_frames_[index].empty()) {
    int ret = get_buffer_(codec_context, av_frame);
    CHECK_EQ(ret, 0);
    ffmpeg_video_frame = new RefCountedAVFrame();
    ffmpeg_video_frame->av_frame_ = *av_frame;
    frame_pool_.push_back(ffmpeg_video_frame);
  } else {
    ffmpeg_video_frame = available_frames_[index].front();
    available_frames_[index].pop_front();
    // We assume |get_buffer| immediately after |release_buffer| will
    // not trigger real buffer allocation. We just use it to fill the
    // correct value inside |pic|.
    release_buffer_(codec_context, &ffmpeg_video_frame->av_frame_);
    get_buffer_(codec_context, av_frame);
    ffmpeg_video_frame->av_frame_ = *av_frame;
  }

  av_frame->opaque = ffmpeg_video_frame;
  av_frame->type = FF_BUFFER_TYPE_USER;
  ffmpeg_video_frame->AddRef();
#endif
  return 0;
}

void FFmpegVideoAllocator::InternalReleaseBuffer(
    AVCodecContext* codec_context, AVFrame* av_frame) {
#ifdef FF_THREAD_FRAME  // Only defined in FFMPEG-MT.
  if (av_frame->opaque == NULL) {
    // This could happened in two scenario:
    // 1. FFMPEG-MT H264 codec seems to allocate one frame during
    //    av_find_stream_info. This happens before we could even
    //    install the custom allocator functions.
    // 2. When clean up time, we reset pic->opaque, and destruct ourselves.
    //    We could not use our own release_buffer function because
    //    handle-delayed-release() is called after we get destructed.
    release_buffer_(codec_context, av_frame);
    return;
  }

  RefCountedAVFrame* ffmpeg_video_frame =
      reinterpret_cast<RefCountedAVFrame*>(av_frame->opaque);
  release_buffer_(codec_context, av_frame);

  // This is required for get_buffer().
  ffmpeg_video_frame->av_frame_.data[0] = NULL;
  get_buffer_(codec_context, &ffmpeg_video_frame->av_frame_);
  int index = codec_index_map_[codec_context];
  if (ffmpeg_video_frame->Release() == 0)
    available_frames_[index].push_back(ffmpeg_video_frame);

  for(int k = 0; k < 4; ++k)
    av_frame->data[k]=NULL;
#endif
}

} // namespace media
