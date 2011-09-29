// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/ffmpeg_video_decode_engine.h"

#include "base/command_line.h"
#include "base/string_number_conversions.h"
#include "base/task.h"
#include "media/base/buffers.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline.h"
#include "media/base/video_util.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_demuxer.h"

namespace media {

FFmpegVideoDecodeEngine::FFmpegVideoDecodeEngine()
    : codec_context_(NULL),
      event_handler_(NULL),
      frame_rate_numerator_(0),
      frame_rate_denominator_(0),
      pending_input_buffers_(0),
      pending_output_buffers_(0),
      output_eos_reached_(false),
      flush_pending_(false) {
}

FFmpegVideoDecodeEngine::~FFmpegVideoDecodeEngine() {
  if (codec_context_) {
    av_free(codec_context_->extradata);
    avcodec_close(codec_context_);
    av_free(codec_context_);
  }
}

void FFmpegVideoDecodeEngine::Initialize(
    MessageLoop* message_loop,
    VideoDecodeEngine::EventHandler* event_handler,
    VideoDecodeContext* context,
    const VideoDecoderConfig& config) {
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

  // TODO(scherkus): should video format get passed in via VideoDecoderConfig?
  codec_context_->pix_fmt = PIX_FMT_YUV420P;
  codec_context_->codec_type = AVMEDIA_TYPE_VIDEO;
  codec_context_->codec_id = VideoCodecToCodecID(config.codec());
  codec_context_->coded_width = config.coded_size().width();
  codec_context_->coded_height = config.coded_size().height();

  frame_rate_numerator_ = config.frame_rate_numerator();
  frame_rate_denominator_ = config.frame_rate_denominator();

  if (config.extra_data() != NULL) {
    codec_context_->extradata_size = config.extra_data_size();
    codec_context_->extradata = reinterpret_cast<uint8_t*>(
        av_malloc(config.extra_data_size() + FF_INPUT_BUFFER_PADDING_SIZE));
    memcpy(codec_context_->extradata, config.extra_data(),
           config.extra_data_size());
    memset(codec_context_->extradata + config.extra_data_size(), '\0',
           FF_INPUT_BUFFER_PADDING_SIZE);
  }

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

  // We don't allocate AVFrame on the stack since different versions of FFmpeg
  // may change the size of AVFrame, causing stack corruption.  The solution is
  // to let FFmpeg allocate the structure via avcodec_alloc_frame().
  av_frame_.reset(avcodec_alloc_frame());
  VideoCodecInfo info;
  info.success = false;
  info.natural_size = config.natural_size();

  // If we do not have enough buffers, we will report error too.
  frame_queue_available_.clear();

  // Create output buffer pool when direct rendering is not used.
  for (size_t i = 0; i < Limits::kMaxVideoFrames; ++i) {
    scoped_refptr<VideoFrame> video_frame =
        VideoFrame::CreateFrame(VideoFrame::YV12,
                                config.visible_rect().width(),
                                config.visible_rect().height(),
                                kNoTimestamp,
                                kNoTimestamp);
    frame_queue_available_.push_back(video_frame);
  }

  codec_context_->thread_count = decode_threads;
  if (codec &&
      avcodec_open(codec_context_, codec) >= 0 &&
      av_frame_.get()) {
    info.success = true;
  }
  event_handler_ = event_handler;
  event_handler_->OnInitializeComplete(info);
}

void FFmpegVideoDecodeEngine::ConsumeVideoSample(
    scoped_refptr<Buffer> buffer) {
  pending_input_buffers_--;
  if (flush_pending_) {
    TryToFinishPendingFlush();
  } else {
    // Otherwise try to decode this buffer.
    DecodeFrame(buffer);
  }
}

void FFmpegVideoDecodeEngine::ProduceVideoFrame(
    scoped_refptr<VideoFrame> frame) {
  // We should never receive NULL frame or EOS frame.
  DCHECK(frame.get() && !frame->IsEndOfStream());

  // Increment pending output buffer count.
  pending_output_buffers_++;

  // Return this frame to available pool after display.
  frame_queue_available_.push_back(frame);

  if (flush_pending_) {
    TryToFinishPendingFlush();
  } else if (!output_eos_reached_) {
    // If we already deliver EOS to renderer, we stop reading new input.
    ReadInput();
  }
}

// Try to decode frame when both input and output are ready.
void FFmpegVideoDecodeEngine::DecodeFrame(scoped_refptr<Buffer> buffer) {
  scoped_refptr<VideoFrame> video_frame;

  // Create a packet for input data.
  // Due to FFmpeg API changes we no longer have const read-only pointers.
  AVPacket packet;
  av_init_packet(&packet);
  packet.data = const_cast<uint8*>(buffer->GetData());
  packet.size = buffer->GetDataSize();

  PipelineStatistics statistics;
  statistics.video_bytes_decoded = buffer->GetDataSize();

  // Let FFmpeg handle presentation timestamp reordering.
  codec_context_->reordered_opaque = buffer->GetTimestamp().InMicroseconds();

  // This is for codecs not using get_buffer to initialize
  // |av_frame_->reordered_opaque|
  av_frame_->reordered_opaque = codec_context_->reordered_opaque;

  int frame_decoded = 0;
  int result = avcodec_decode_video2(codec_context_,
                                     av_frame_.get(),
                                     &frame_decoded,
                                     &packet);
  // Log the problem if we can't decode a video frame and exit early.
  if (result < 0) {
    LOG(ERROR) << "Error decoding a video frame with timestamp: "
               << buffer->GetTimestamp().InMicroseconds() << " us, duration: "
               << buffer->GetDuration().InMicroseconds() << " us, packet size: "
               << buffer->GetDataSize() << " bytes";
    event_handler_->OnError();
    return;
  }

  // If frame_decoded == 0, then no frame was produced.
  // In this case, if we already begin to flush codec with empty
  // input packet at the end of input stream, the first time we
  // encounter frame_decoded == 0 signal output frame had been
  // drained, we mark the flag. Otherwise we read from demuxer again.
  if (frame_decoded == 0) {
    if (buffer->IsEndOfStream()) {  // We had started flushing.
      event_handler_->ConsumeVideoFrame(video_frame, statistics);
      output_eos_reached_ = true;
    } else {
      ReadInput();
    }
    return;
  }

  // TODO(fbarchard): Work around for FFmpeg http://crbug.com/27675
  // The decoder is in a bad state and not decoding correctly.
  // Checking for NULL avoids a crash in CopyPlane().
  if (!av_frame_->data[VideoFrame::kYPlane] ||
      !av_frame_->data[VideoFrame::kUPlane] ||
      !av_frame_->data[VideoFrame::kVPlane]) {
    event_handler_->OnError();
    return;
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

  base::TimeDelta timestamp =
      base::TimeDelta::FromMicroseconds(av_frame_->reordered_opaque);
  base::TimeDelta duration =
      ConvertFromTimeBase(doubled_time_base, 2 + av_frame_->repeat_pict);

  // Available frame is guaranteed, because we issue as much reads as
  // available frame, except the case of |frame_decoded| == 0, which
  // implies decoder order delay, and force us to read more inputs.
  DCHECK(frame_queue_available_.size());
  video_frame = frame_queue_available_.front();
  frame_queue_available_.pop_front();

  // Copy the frame data since FFmpeg reuses internal buffers for AVFrame
  // output, meaning the data is only valid until the next
  // avcodec_decode_video() call.
  int y_rows = codec_context_->height;
  int uv_rows = codec_context_->height / 2;
  CopyYPlane(av_frame_->data[0], av_frame_->linesize[0], y_rows, video_frame);
  CopyUPlane(av_frame_->data[1], av_frame_->linesize[1], uv_rows, video_frame);
  CopyVPlane(av_frame_->data[2], av_frame_->linesize[2], uv_rows, video_frame);

  video_frame->SetTimestamp(timestamp);
  video_frame->SetDuration(duration);

  pending_output_buffers_--;
  event_handler_->ConsumeVideoFrame(video_frame, statistics);
}

void FFmpegVideoDecodeEngine::Uninitialize() {
  event_handler_->OnUninitializeComplete();
}

void FFmpegVideoDecodeEngine::Flush() {
  avcodec_flush_buffers(codec_context_);
  flush_pending_ = true;
  TryToFinishPendingFlush();
}

void FFmpegVideoDecodeEngine::TryToFinishPendingFlush() {
  DCHECK(flush_pending_);

  // We consider ourself flushed when there is no pending input buffers
  // and output buffers, which implies that all buffers had been returned
  // to its owner.
  if (!pending_input_buffers_ && !pending_output_buffers_) {
    // Try to finish flushing and notify pipeline.
    flush_pending_ = false;
    event_handler_->OnFlushComplete();
  }
}

void FFmpegVideoDecodeEngine::Seek() {
  // After a seek, output stream no longer considered as EOS.
  output_eos_reached_ = false;

  // The buffer provider is assumed to perform pre-roll operation.
  for (unsigned int i = 0; i < Limits::kMaxVideoFrames; ++i)
    ReadInput();

  event_handler_->OnSeekComplete();
}

void FFmpegVideoDecodeEngine::ReadInput() {
  DCHECK_EQ(output_eos_reached_, false);
  pending_input_buffers_++;
  event_handler_->ProduceVideoSample(NULL);
}

}  // namespace media

// Disable refcounting for this object because this object only lives
// on the video decoder thread and there's no need to refcount it.
DISABLE_RUNNABLE_METHOD_REFCOUNT(media::FFmpegVideoDecodeEngine);
