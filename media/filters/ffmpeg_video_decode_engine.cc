// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "media/filters/ffmpeg_video_decode_engine.h"

#include "base/task.h"
#include "media/base/callback.h"
#include "media/base/video_frame_impl.h"
#include "media/ffmpeg/ffmpeg_util.h"
#include "media/filters/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"

namespace media {

FFmpegVideoDecodeEngine::FFmpegVideoDecodeEngine()
    : codec_context_(NULL),
      state_(kCreated) {
}

FFmpegVideoDecodeEngine::~FFmpegVideoDecodeEngine() {
}

void FFmpegVideoDecodeEngine::Initialize(AVStream* stream, Task* done_cb) {
  AutoTaskRunner done_runner(done_cb);

  // Always try to use two threads for video decoding.  There is little reason
  // not to since current day CPUs tend to be multi-core and we measured
  // performance benefits on older machines such as P4s with hyperthreading.
  //
  // Handling decoding on separate threads also frees up the pipeline thread to
  // continue processing. Although it'd be nice to have the option of a single
  // decoding thread, FFmpeg treats having one thread the same as having zero
  // threads (i.e., avcodec_decode_video() will execute on the calling thread).
  // Yet another reason for having two threads :)
  //
  // TODO(scherkus): some video codecs might not like avcodec_thread_init()
  // being called on them... should attempt to find out which ones those are!
  static const int kDecodeThreads = 2;

  CHECK(state_ == kCreated);

  codec_context_ = stream->codec;
  codec_context_->flags2 |= CODEC_FLAG2_FAST;  // Enable faster H264 decode.
  // Enable motion vector search (potentially slow), strong deblocking filter
  // for damaged macroblocks, and set our error detection sensitivity.
  codec_context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  codec_context_->error_recognition = FF_ER_CAREFUL;

  // Serialize calls to avcodec_open().
  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  {
    AutoLock auto_lock(FFmpegLock::get()->lock());
    if (codec &&
        avcodec_thread_init(codec_context_, kDecodeThreads) >= 0 &&
        avcodec_open(codec_context_, codec) >= 0) {
      state_ = kNormal;
    } else {
      state_ = kError;
    }
  }
}

// Decodes one frame of video with the given buffer.
void FFmpegVideoDecodeEngine::DecodeFrame(const Buffer& buffer,
                                          AVFrame* yuv_frame,
                                          bool* got_frame,
                                          Task* done_cb) {
  AutoTaskRunner done_runner(done_cb);

  // Create a packet for input data.
  // Due to FFmpeg API changes we no longer have const read-only pointers.
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8*>(buffer.GetData());
  packet.size = buffer.GetDataSize();

  // We don't allocate AVFrame on the stack since different versions of FFmpeg
  // may change the size of AVFrame, causing stack corruption.  The solution is
  // to let FFmpeg allocate the structure via avcodec_alloc_frame().
  int frame_decoded = 0;
  int result =
      avcodec_decode_video2(codec_context_, yuv_frame, &frame_decoded, &packet);

  // Log the problem if we can't decode a video frame and exit early.
  if (result < 0) {
    LOG(INFO) << "Error decoding a video frame with timestamp: "
              << buffer.GetTimestamp().InMicroseconds() << " us"
              << " , duration: "
              << buffer.GetDuration().InMicroseconds() << " us"
              << " , packet size: "
              << buffer.GetDataSize() << " bytes";
    *got_frame = false;
  } else {
    // If frame_decoded == 0, then no frame was produced.
    *got_frame = frame_decoded != 0;
  }
}

void FFmpegVideoDecodeEngine::Flush(Task* done_cb) {
  AutoTaskRunner done_runner(done_cb);

  avcodec_flush_buffers(codec_context_);
}

VideoSurface::Format FFmpegVideoDecodeEngine::GetSurfaceFormat() const {
  // J (Motion JPEG) versions of YUV are full range 0..255.
  // Regular (MPEG) YUV is 16..240.
  // For now we will ignore the distinction and treat them the same.
  switch (codec_context_->pix_fmt) {
    case PIX_FMT_YUV420P:
    case PIX_FMT_YUVJ420P:
      return VideoSurface::YV12;
      break;
    case PIX_FMT_YUV422P:
    case PIX_FMT_YUVJ422P:
      return VideoSurface::YV16;
      break;
    default:
      // TODO(scherkus): More formats here?
      return VideoSurface::INVALID;
  }
}

}  // namespace media
