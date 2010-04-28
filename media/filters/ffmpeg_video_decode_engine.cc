// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_video_decode_engine.h"

#include "base/task.h"
#include "media/base/buffers.h"
#include "media/base/callback.h"
#include "media/base/limits.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_util.h"
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
  static const int kDecodeThreads = 2;

  CHECK(state_ == kCreated);

  codec_context_ = stream->codec;
  codec_context_->flags2 |= CODEC_FLAG2_FAST;  // Enable faster H264 decode.
  // Enable motion vector search (potentially slow), strong deblocking filter
  // for damaged macroblocks, and set our error detection sensitivity.
  codec_context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  codec_context_->error_recognition = FF_ER_CAREFUL;

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);

  // TODO(fbarchard): On next ffmpeg roll, retest Theora multi-threading.
  int decode_threads = (codec_context_->codec_id == CODEC_ID_THEORA)
      ? 1 : kDecodeThreads;

  // We don't allocate AVFrame on the stack since different versions of FFmpeg
  // may change the size of AVFrame, causing stack corruption.  The solution is
  // to let FFmpeg allocate the structure via avcodec_alloc_frame().
  av_frame_.reset(avcodec_alloc_frame());

  if (codec &&
      avcodec_thread_init(codec_context_, decode_threads) >= 0 &&
      avcodec_open(codec_context_, codec) >= 0 &&
      av_frame_.get()) {
    state_ = kNormal;
  } else {
    state_ = kError;
  }
}

static void CopyPlane(size_t plane,
                      scoped_refptr<VideoFrame> video_frame,
                      const AVFrame* frame) {
  DCHECK(video_frame->width() % 2 == 0);
  const uint8* source = frame->data[plane];
  const size_t source_stride = frame->linesize[plane];
  uint8* dest = video_frame->data(plane);
  const size_t dest_stride = video_frame->stride(plane);
  size_t bytes_per_line = video_frame->width();
  size_t copy_lines = video_frame->height();
  if (plane != VideoFrame::kYPlane) {
    bytes_per_line /= 2;
    if (video_frame->format() == VideoFrame::YV12) {
      copy_lines = (copy_lines + 1) / 2;
    }
  }
  DCHECK(bytes_per_line <= source_stride && bytes_per_line <= dest_stride);
  for (size_t i = 0; i < copy_lines; ++i) {
    memcpy(dest, source, bytes_per_line);
    source += source_stride;
    dest += dest_stride;
  }
}

// Decodes one frame of video with the given buffer.
void FFmpegVideoDecodeEngine::DecodeFrame(
    Buffer* buffer,
    scoped_refptr<VideoFrame>* video_frame,
    bool* got_frame,
    Task* done_cb) {
  AutoTaskRunner done_runner(done_cb);
  *got_frame = false;
  *video_frame = NULL;

  // Create a packet for input data.
  // Due to FFmpeg API changes we no longer have const read-only pointers.
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8*>(buffer->GetData());
  packet.size = buffer->GetDataSize();

  int frame_decoded = 0;
  int result = avcodec_decode_video2(codec_context_,
                                     av_frame_.get(),
                                     &frame_decoded,
                                     &packet);

  // Log the problem if we can't decode a video frame and exit early.
  if (result < 0) {
    LOG(INFO) << "Error decoding a video frame with timestamp: "
              << buffer->GetTimestamp().InMicroseconds() << " us"
              << " , duration: "
              << buffer->GetDuration().InMicroseconds() << " us"
              << " , packet size: "
              << buffer->GetDataSize() << " bytes";
    *got_frame = false;
    return;
  }

  // If frame_decoded == 0, then no frame was produced.
  *got_frame = frame_decoded != 0;
  if (!*got_frame) {
    return;
  }

  // TODO(fbarchard): Work around for FFmpeg http://crbug.com/27675
  // The decoder is in a bad state and not decoding correctly.
  // Checking for NULL avoids a crash in CopyPlane().
  if (!av_frame_->data[VideoFrame::kYPlane] ||
      !av_frame_->data[VideoFrame::kUPlane] ||
      !av_frame_->data[VideoFrame::kVPlane]) {
    // TODO(jiesun): this is also an error case handled as normal.
    *got_frame = false;
    return;
  }

  VideoFrame::CreateFrame(GetSurfaceFormat(),
                          codec_context_->width,
                          codec_context_->height,
                          StreamSample::kInvalidTimestamp,
                          StreamSample::kInvalidTimestamp,
                          video_frame);
  if (!video_frame->get()) {
    // TODO(jiesun): this is also an error case handled as normal.
    *got_frame = false;
    return;
  }

  // Copy the frame data since FFmpeg reuses internal buffers for AVFrame
  // output, meaning the data is only valid until the next
  // avcodec_decode_video() call.
  // TODO(scherkus): figure out pre-allocation/buffer cycling scheme.
  // TODO(scherkus): is there a cleaner way to figure out the # of planes?
  CopyPlane(VideoFrame::kYPlane, video_frame->get(), av_frame_.get());
  CopyPlane(VideoFrame::kUPlane, video_frame->get(), av_frame_.get());
  CopyPlane(VideoFrame::kVPlane, video_frame->get(), av_frame_.get());

  DCHECK_LE(av_frame_->repeat_pict, 2);  // Sanity check.
  (*video_frame)->SetRepeatCount(av_frame_->repeat_pict);
}

void FFmpegVideoDecodeEngine::Flush(Task* done_cb) {
  AutoTaskRunner done_runner(done_cb);

  avcodec_flush_buffers(codec_context_);
}

VideoFrame::Format FFmpegVideoDecodeEngine::GetSurfaceFormat() const {
  // J (Motion JPEG) versions of YUV are full range 0..255.
  // Regular (MPEG) YUV is 16..240.
  // For now we will ignore the distinction and treat them the same.
  switch (codec_context_->pix_fmt) {
    case PIX_FMT_YUV420P:
    case PIX_FMT_YUVJ420P:
      return VideoFrame::YV12;
      break;
    case PIX_FMT_YUV422P:
    case PIX_FMT_YUVJ422P:
      return VideoFrame::YV16;
      break;
    default:
      // TODO(scherkus): More formats here?
      return VideoFrame::INVALID;
  }
}

}  // namespace media
