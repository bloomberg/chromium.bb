// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/ffmpeg_video_decode_engine.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "media/base/buffers.h"
#include "media/base/media_switches.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_util.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

FFmpegVideoDecodeEngine::FFmpegVideoDecodeEngine()
    : codec_context_(NULL),
      av_frame_(NULL),
      frame_rate_numerator_(0),
      frame_rate_denominator_(0) {
}

FFmpegVideoDecodeEngine::~FFmpegVideoDecodeEngine() {
  Uninitialize();
}

bool FFmpegVideoDecodeEngine::Initialize(const VideoDecoderConfig& config) {
  frame_rate_numerator_ = config.frame_rate_numerator();
  frame_rate_denominator_ = config.frame_rate_denominator();

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

  // Initialize AVCodecContext structure.
  codec_context_ = avcodec_alloc_context();
  VideoDecoderConfigToAVCodecContext(config, codec_context_);

  // Enable motion vector search (potentially slow), strong deblocking filter
  // for damaged macroblocks, and set our error detection sensitivity.
  codec_context_->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  codec_context_->error_recognition = FF_ER_CAREFUL;

  AVCodec* codec = avcodec_find_decoder(codec_context_->codec_id);

  // TODO(fbarchard): Improve thread logic based on size / codec.
  // TODO(fbarchard): Fix bug affecting video-cookie.html
  // 07/21/11(ihf): Still about 20 failures when enabling.
  int decode_threads = (codec_context_->codec_id == CODEC_ID_THEORA) ?
      1 : kDecodeThreads;

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  std::string threads(cmd_line->GetSwitchValueASCII(switches::kVideoThreads));
  if ((!threads.empty() &&
       !base::StringToInt(threads, &decode_threads)) ||
      decode_threads < 0 || decode_threads > kMaxDecodeThreads) {
    decode_threads = kDecodeThreads;
  }

  codec_context_->thread_count = decode_threads;

  av_frame_ = avcodec_alloc_frame();

  // Open the codec!
  return codec && avcodec_open(codec_context_, codec) >= 0;
}

void FFmpegVideoDecodeEngine::Uninitialize() {
  if (codec_context_) {
    av_free(codec_context_->extradata);
    avcodec_close(codec_context_);
    av_free(codec_context_);
    codec_context_ = NULL;
  }
  if (av_frame_) {
    av_free(av_frame_);
    av_frame_ = NULL;
  }
  frame_rate_numerator_ = 0;
  frame_rate_denominator_ = 0;
}

bool FFmpegVideoDecodeEngine::Decode(const scoped_refptr<Buffer>& buffer,
                                     scoped_refptr<VideoFrame>* video_frame) {
  DCHECK(video_frame);

  // Create a packet for input data.
  // Due to FFmpeg API changes we no longer have const read-only pointers.
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8*>(buffer->GetData());
  packet.size = buffer->GetDataSize();

  // Let FFmpeg handle presentation timestamp reordering.
  codec_context_->reordered_opaque = buffer->GetTimestamp().InMicroseconds();

  // This is for codecs not using get_buffer to initialize
  // |av_frame_->reordered_opaque|
  av_frame_->reordered_opaque = codec_context_->reordered_opaque;

  int frame_decoded = 0;
  int result = avcodec_decode_video2(codec_context_,
                                     av_frame_,
                                     &frame_decoded,
                                     &packet);
  // Log the problem if we can't decode a video frame and exit early.
  if (result < 0) {
    LOG(ERROR) << "Error decoding a video frame with timestamp: "
               << buffer->GetTimestamp().InMicroseconds() << " us, duration: "
               << buffer->GetDuration().InMicroseconds() << " us, packet size: "
               << buffer->GetDataSize() << " bytes";
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

  // We've got a frame! Make sure we have a place to store it.
  *video_frame = AllocateVideoFrame();
  if (!(*video_frame)) {
    LOG(ERROR) << "Failed to allocate video frame";
    return false;
  }

  // Determine timestamp and calculate the duration based on the repeat picture
  // count.  According to FFmpeg docs, the total duration can be calculated as
  // follows:
  //   fps = 1 / time_base
  //
  //   duration = (1 / fps) + (repeat_pict) / (2 * fps)
  //            = (2 + repeat_pict) / (2 * fps)
  //            = (2 + repeat_pict) / (2 * (1 / time_base))
  DCHECK_LE(av_frame_->repeat_pict, 2);  // Sanity check.
  AVRational doubled_time_base;
  doubled_time_base.num = frame_rate_denominator_;
  doubled_time_base.den = frame_rate_numerator_ * 2;

  (*video_frame)->SetTimestamp(
      base::TimeDelta::FromMicroseconds(av_frame_->reordered_opaque));
  (*video_frame)->SetDuration(
      ConvertFromTimeBase(doubled_time_base, 2 + av_frame_->repeat_pict));

  // Copy the frame data since FFmpeg reuses internal buffers for AVFrame
  // output, meaning the data is only valid until the next
  // avcodec_decode_video() call.
  int y_rows = codec_context_->height;
  int uv_rows = codec_context_->height;
  if (codec_context_->pix_fmt == PIX_FMT_YUV420P) {
    uv_rows /= 2;
  }

  CopyYPlane(av_frame_->data[0], av_frame_->linesize[0], y_rows, *video_frame);
  CopyUPlane(av_frame_->data[1], av_frame_->linesize[1], uv_rows, *video_frame);
  CopyVPlane(av_frame_->data[2], av_frame_->linesize[2], uv_rows, *video_frame);

  return true;
}

void FFmpegVideoDecodeEngine::Flush() {
  avcodec_flush_buffers(codec_context_);
}

scoped_refptr<VideoFrame> FFmpegVideoDecodeEngine::AllocateVideoFrame() {
  VideoFrame::Format format = PixelFormatToVideoFormat(codec_context_->pix_fmt);
  size_t width = codec_context_->width;
  size_t height = codec_context_->height;

  return VideoFrame::CreateFrame(format, width, height,
                                 kNoTimestamp, kNoTimestamp);
}

}  // namespace media
