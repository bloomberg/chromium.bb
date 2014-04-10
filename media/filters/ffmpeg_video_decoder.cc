// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/ffmpeg_video_decoder.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"

namespace media {

// Always try to use three threads for video decoding.  There is little reason
// not to since current day CPUs tend to be multi-core and we measured
// performance benefits on older machines such as P4s with hyperthreading.
//
// Handling decoding on separate threads also frees up the pipeline thread to
// continue processing. Although it'd be nice to have the option of a single
// decoding thread, FFmpeg treats having one thread the same as having zero
// threads (i.e., avcodec_decode_video() will execute on the calling thread).
// Yet another reason for having two threads :)
static const int kDecodeThreads = 2;
static const int kMaxDecodeThreads = 16;

// Returns the number of threads given the FFmpeg CodecID. Also inspects the
// command line for a valid --video-threads flag.
static int GetThreadCount(AVCodecID codec_id) {
  // Refer to http://crbug.com/93932 for tsan suppressions on decoding.
  int decode_threads = kDecodeThreads;

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  std::string threads(cmd_line->GetSwitchValueASCII(switches::kVideoThreads));
  if (threads.empty() || !base::StringToInt(threads, &decode_threads))
    return decode_threads;

  decode_threads = std::max(decode_threads, 0);
  decode_threads = std::min(decode_threads, kMaxDecodeThreads);
  return decode_threads;
}

FFmpegVideoDecoder::FFmpegVideoDecoder(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner), state_(kUninitialized) {}

int FFmpegVideoDecoder::GetVideoBuffer(AVCodecContext* codec_context,
                                       AVFrame* frame) {
  // Don't use |codec_context_| here! With threaded decoding,
  // it will contain unsynchronized width/height/pix_fmt values,
  // whereas |codec_context| contains the current threads's
  // updated width/height/pix_fmt, which can change for adaptive
  // content.
  VideoFrame::Format format = PixelFormatToVideoFormat(codec_context->pix_fmt);
  if (format == VideoFrame::UNKNOWN)
    return AVERROR(EINVAL);
  DCHECK(format == VideoFrame::YV12 || format == VideoFrame::YV16 ||
         format == VideoFrame::YV12J);

  gfx::Size size(codec_context->width, codec_context->height);
  int ret;
  if ((ret = av_image_check_size(size.width(), size.height(), 0, NULL)) < 0)
    return ret;

  gfx::Size natural_size;
  if (codec_context->sample_aspect_ratio.num > 0) {
    natural_size = GetNaturalSize(size,
                                  codec_context->sample_aspect_ratio.num,
                                  codec_context->sample_aspect_ratio.den);
  } else {
    natural_size = config_.natural_size();
  }

  if (!VideoFrame::IsValidConfig(format, size, gfx::Rect(size), natural_size))
    return AVERROR(EINVAL);

  scoped_refptr<VideoFrame> video_frame =
      frame_pool_.CreateFrame(format, size, gfx::Rect(size),
                              natural_size, kNoTimestamp());

  for (int i = 0; i < 3; i++) {
    frame->base[i] = video_frame->data(i);
    frame->data[i] = video_frame->data(i);
    frame->linesize[i] = video_frame->stride(i);
  }

  frame->opaque = NULL;
  video_frame.swap(reinterpret_cast<VideoFrame**>(&frame->opaque));
  frame->type = FF_BUFFER_TYPE_USER;
  frame->width = codec_context->width;
  frame->height = codec_context->height;
  frame->format = codec_context->pix_fmt;

  return 0;
}

static int GetVideoBufferImpl(AVCodecContext* s, AVFrame* frame) {
  FFmpegVideoDecoder* decoder = static_cast<FFmpegVideoDecoder*>(s->opaque);
  return decoder->GetVideoBuffer(s, frame);
}

static void ReleaseVideoBufferImpl(AVCodecContext* s, AVFrame* frame) {
  scoped_refptr<VideoFrame> video_frame;
  video_frame.swap(reinterpret_cast<VideoFrame**>(&frame->opaque));

  // The FFmpeg API expects us to zero the data pointers in
  // this callback
  memset(frame->data, 0, sizeof(frame->data));
  frame->opaque = NULL;
}

void FFmpegVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                    const PipelineStatusCB& status_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(decode_cb_.is_null());
  DCHECK(!config.is_encrypted());

  FFmpegGlue::InitializeFFmpeg();

  config_ = config;
  PipelineStatusCB initialize_cb = BindToCurrentLoop(status_cb);

  if (!config.IsValidConfig() || !ConfigureDecoder()) {
    initialize_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  // Success!
  state_ = kNormal;
  initialize_cb.Run(PIPELINE_OK);
}

void FFmpegVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                                const DecodeCB& decode_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(!decode_cb.is_null());
  CHECK_NE(state_, kUninitialized);
  CHECK(decode_cb_.is_null()) << "Overlapping decodes are not supported.";
  decode_cb_ = BindToCurrentLoop(decode_cb);

  if (state_ == kError) {
    base::ResetAndReturn(&decode_cb_).Run(kDecodeError, NULL);
    return;
  }

  // Return empty frames if decoding has finished.
  if (state_ == kDecodeFinished) {
    base::ResetAndReturn(&decode_cb_).Run(kOk, VideoFrame::CreateEOSFrame());
    return;
  }

  DecodeBuffer(buffer);
}

void FFmpegVideoDecoder::Reset(const base::Closure& closure) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK(decode_cb_.is_null());

  avcodec_flush_buffers(codec_context_.get());
  state_ = kNormal;
  task_runner_->PostTask(FROM_HERE, closure);
}

void FFmpegVideoDecoder::Stop() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  if (state_ == kUninitialized)
    return;

  ReleaseFFmpegResources();
  state_ = kUninitialized;
}

FFmpegVideoDecoder::~FFmpegVideoDecoder() {
  DCHECK_EQ(kUninitialized, state_);
  DCHECK(!codec_context_);
  DCHECK(!av_frame_);
}

void FFmpegVideoDecoder::DecodeBuffer(
    const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  DCHECK_NE(state_, kUninitialized);
  DCHECK_NE(state_, kDecodeFinished);
  DCHECK_NE(state_, kError);
  DCHECK(!decode_cb_.is_null());
  DCHECK(buffer);

  // During decode, because reads are issued asynchronously, it is possible to
  // receive multiple end of stream buffers since each decode is acked. When the
  // first end of stream buffer is read, FFmpeg may still have frames queued
  // up in the decoder so we need to go through the decode loop until it stops
  // giving sensible data.  After that, the decoder should output empty
  // frames.  There are three states the decoder can be in:
  //
  //   kNormal: This is the starting state. Buffers are decoded. Decode errors
  //            are discarded.
  //   kFlushCodec: There isn't any more input data. Call avcodec_decode_video2
  //                until no more data is returned to flush out remaining
  //                frames. The input buffer is ignored at this point.
  //   kDecodeFinished: All calls return empty frames.
  //   kError: Unexpected error happened.
  //
  // These are the possible state transitions.
  //
  // kNormal -> kFlushCodec:
  //     When buffer->end_of_stream() is first true.
  // kNormal -> kError:
  //     A decoding error occurs and decoding needs to stop.
  // kFlushCodec -> kDecodeFinished:
  //     When avcodec_decode_video2() returns 0 data.
  // kFlushCodec -> kError:
  //     When avcodec_decode_video2() errors out.
  // (any state) -> kNormal:
  //     Any time Reset() is called.

  // Transition to kFlushCodec on the first end of stream buffer.
  if (state_ == kNormal && buffer->end_of_stream()) {
    state_ = kFlushCodec;
  }

  scoped_refptr<VideoFrame> video_frame;
  if (!FFmpegDecode(buffer, &video_frame)) {
    state_ = kError;
    base::ResetAndReturn(&decode_cb_).Run(kDecodeError, NULL);
    return;
  }

  if (!video_frame.get()) {
    if (state_ == kFlushCodec) {
      DCHECK(buffer->end_of_stream());
      state_ = kDecodeFinished;
      base::ResetAndReturn(&decode_cb_)
          .Run(kOk, VideoFrame::CreateEOSFrame());
      return;
    }

    base::ResetAndReturn(&decode_cb_).Run(kNotEnoughData, NULL);
    return;
  }

  base::ResetAndReturn(&decode_cb_).Run(kOk, video_frame);
}

bool FFmpegVideoDecoder::FFmpegDecode(
    const scoped_refptr<DecoderBuffer>& buffer,
    scoped_refptr<VideoFrame>* video_frame) {
  DCHECK(video_frame);

  // Reset frame to default values.
  avcodec_get_frame_defaults(av_frame_.get());

  // Create a packet for input data.
  // Due to FFmpeg API changes we no longer have const read-only pointers.
  AVPacket packet;
  av_init_packet(&packet);
  if (buffer->end_of_stream()) {
    packet.data = NULL;
    packet.size = 0;
  } else {
    packet.data = const_cast<uint8*>(buffer->data());
    packet.size = buffer->data_size();

    // Let FFmpeg handle presentation timestamp reordering.
    codec_context_->reordered_opaque = buffer->timestamp().InMicroseconds();

    // This is for codecs not using get_buffer to initialize
    // |av_frame_->reordered_opaque|
    av_frame_->reordered_opaque = codec_context_->reordered_opaque;
  }

  int frame_decoded = 0;
  int result = avcodec_decode_video2(codec_context_.get(),
                                     av_frame_.get(),
                                     &frame_decoded,
                                     &packet);
  // Log the problem if we can't decode a video frame and exit early.
  if (result < 0) {
    LOG(ERROR) << "Error decoding video: " << buffer->AsHumanReadableString();
    *video_frame = NULL;
    return false;
  }

  // If no frame was produced then signal that more data is required to
  // produce more frames. This can happen under two circumstances:
  //   1) Decoder was recently initialized/flushed
  //   2) End of stream was reached and all internal frames have been output
  if (frame_decoded == 0) {
    *video_frame = NULL;
    return true;
  }

  // TODO(fbarchard): Work around for FFmpeg http://crbug.com/27675
  // The decoder is in a bad state and not decoding correctly.
  // Checking for NULL avoids a crash in CopyPlane().
  if (!av_frame_->data[VideoFrame::kYPlane] ||
      !av_frame_->data[VideoFrame::kUPlane] ||
      !av_frame_->data[VideoFrame::kVPlane]) {
    LOG(ERROR) << "Video frame was produced yet has invalid frame data.";
    *video_frame = NULL;
    return false;
  }

  if (!av_frame_->opaque) {
    LOG(ERROR) << "VideoFrame object associated with frame data not set.";
    return false;
  }
  *video_frame = static_cast<VideoFrame*>(av_frame_->opaque);

  (*video_frame)->set_timestamp(
      base::TimeDelta::FromMicroseconds(av_frame_->reordered_opaque));

  return true;
}

void FFmpegVideoDecoder::ReleaseFFmpegResources() {
  codec_context_.reset();
  av_frame_.reset();
}

bool FFmpegVideoDecoder::ConfigureDecoder() {
  // Release existing decoder resources if necessary.
  ReleaseFFmpegResources();

  // Initialize AVCodecContext structure.
  codec_context_.reset(avcodec_alloc_context3(NULL));
  VideoDecoderConfigToAVCodecContext(config_, codec_context_.get());

  // Enable motion vector search (potentially slow), strong deblocking filter
  // for damaged macroblocks, and set our error detection sensitivity.
  codec_context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  codec_context_->thread_count = GetThreadCount(codec_context_->codec_id);
  codec_context_->opaque = this;
  codec_context_->flags |= CODEC_FLAG_EMU_EDGE;
  codec_context_->get_buffer = GetVideoBufferImpl;
  codec_context_->release_buffer = ReleaseVideoBufferImpl;

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);
  if (!codec || avcodec_open2(codec_context_.get(), codec, NULL) < 0) {
    ReleaseFFmpegResources();
    return false;
  }

  av_frame_.reset(av_frame_alloc());
  return true;
}

}  // namespace media
