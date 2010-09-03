// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.


#include "chrome/renderer/media/ipc_video_decoder.h"

#include "base/task.h"
#include "chrome/renderer/ggl/ggl.h"
#include "media/base/callback.h"
#include "media/base/filters.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/base/media_format.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_util.h"
#include "media/filters/ffmpeg_interfaces.h"

namespace media {

IpcVideoDecoder::IpcVideoDecoder(MessageLoop* message_loop)
    : width_(0),
      height_(0),
      state_(kUnInitialized),
      pending_reads_(0),
      pending_requests_(0),
      renderer_thread_message_loop_(message_loop) {
}

IpcVideoDecoder::~IpcVideoDecoder() {
}

void IpcVideoDecoder::Initialize(DemuxerStream* demuxer_stream,
                                 FilterCallback* callback) {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::Initialize,
                          demuxer_stream,
                          callback));
    return;
  }

  CHECK(!demuxer_stream_);
  demuxer_stream_ = demuxer_stream;
  initialize_callback_.reset(callback);

  // We require bit stream converter for openmax hardware decoder.
  // TODO(hclam): This is a wrong place to initialize the demuxer stream's
  // bitstream converter.
  demuxer_stream->EnableBitstreamConverter();

  // Get the AVStream by querying for the provider interface.
  AVStreamProvider* av_stream_provider;
  if (!demuxer_stream->QueryInterface(&av_stream_provider)) {
    GpuVideoDecoderInitDoneParam param;
    OnInitializeDone(false, param);
    return;
  }

  AVStream* av_stream = av_stream_provider->GetAVStream();
  width_ = av_stream->codec->width;
  height_ = av_stream->codec->height;

  // TODO(hclam): Pass an actual context instead of NULL.
  gpu_video_decoder_host_ = ggl::CreateVideoDecoder(NULL);

  // Initialize hardware decoder.
  GpuVideoDecoderInitParam param = {0};
  param.width_ = width_;
  param.height_ = height_;
  if (!gpu_video_decoder_host_->Initialize(this, param)) {
    GpuVideoDecoderInitDoneParam param;
    OnInitializeDone(false, param);
  }
}

void IpcVideoDecoder::OnInitializeDone(
    bool success, const GpuVideoDecoderInitDoneParam& param) {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::OnInitializeDone,
                          success,
                          param));
    return;
  }

  AutoCallbackRunner done_runner(initialize_callback_.release());

  if (success) {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              mime_type::kUncompressedVideo);
    media_format_.SetAsInteger(MediaFormat::kWidth, width_);
    media_format_.SetAsInteger(MediaFormat::kHeight, height_);
    media_format_.SetAsInteger(MediaFormat::kSurfaceType,
                               static_cast<int>(param.surface_type_));
    media_format_.SetAsInteger(MediaFormat::kSurfaceFormat,
                               static_cast<int>(param.format_));
    state_ = kPlaying;
  } else {
    LOG(ERROR) << "IpcVideoDecoder initialization failed!";
    host()->SetError(PIPELINE_ERROR_DECODE);
  }
}

void IpcVideoDecoder::Stop(FilterCallback* callback) {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::Stop,
                          callback));
    return;
  }

  stop_callback_.reset(callback);
  if (!gpu_video_decoder_host_->Uninitialize()) {
    LOG(ERROR) << "gpu video decoder destroy failed";
    IpcVideoDecoder::OnUninitializeDone();
  }
}

void IpcVideoDecoder::OnUninitializeDone() {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::OnUninitializeDone));
    return;
  }

  AutoCallbackRunner done_runner(stop_callback_.release());

  state_ = kStopped;
}

void IpcVideoDecoder::Pause(FilterCallback* callback) {
  Flush(callback);  // TODO(jiesun): move this to flush().
}

void IpcVideoDecoder::Flush(FilterCallback* callback) {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::Flush,
                          callback));
    return;
  }

  state_ = kFlushing;

  flush_callback_.reset(callback);

  if (!gpu_video_decoder_host_->Flush()) {
    LOG(ERROR) << "gpu video decoder flush failed";
    OnFlushDone();
  }
}

void IpcVideoDecoder::OnFlushDone() {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::OnFlushDone));
    return;
  }

  if (pending_reads_ == 0 && pending_requests_ == 0 && flush_callback_.get()) {
    flush_callback_->Run();
    flush_callback_.reset();
  }
}

void IpcVideoDecoder::Seek(base::TimeDelta time, FilterCallback* callback) {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::Seek,
                          time,
                          callback));
    return;
  }

  OnSeekComplete(callback);
}

void IpcVideoDecoder::OnSeekComplete(FilterCallback* callback) {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::OnSeekComplete,
                          callback));
    return;
  }

  AutoCallbackRunner done_runner(callback);

  state_ = kPlaying;

  for (int i = 0; i < 20; ++i) {
    demuxer_stream_->Read(
        NewCallback(this,
                    &IpcVideoDecoder::OnReadComplete));
    ++pending_reads_;
  }
}

void IpcVideoDecoder::OnReadComplete(Buffer* buffer) {
  scoped_refptr<Buffer> buffer_ref = buffer;
  ReadCompleteTask(buffer_ref);
}

void IpcVideoDecoder::ReadCompleteTask(scoped_refptr<Buffer> buffer) {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::ReadCompleteTask,
                          buffer));
    return;
  }

  DCHECK_GT(pending_reads_, 0u);
  --pending_reads_;

  if (state_ == kStopped || state_ == kEnded) {
    // Just discard the input buffers
    return;
  }

  if (state_ == kFlushing) {
    if (pending_reads_ == 0 && pending_requests_ == 0) {
      CHECK(flush_callback_.get());
      flush_callback_->Run();
      flush_callback_.reset();
      state_ = kPlaying;
    }
    return;
  }
  // Transition to kFlushCodec on the first end of input stream buffer.
  if (state_ == kPlaying && buffer->IsEndOfStream()) {
    state_ = kFlushCodec;
  }

  gpu_video_decoder_host_->EmptyThisBuffer(buffer);
}

void IpcVideoDecoder::FillThisBuffer(scoped_refptr<VideoFrame> video_frame) {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::FillThisBuffer,
                          video_frame));
    return;
  }

  // Synchronized flushing before stop should prevent this.
  CHECK_NE(state_, kStopped);

  // Notify decode engine the available of new frame.
  ++pending_requests_;
  gpu_video_decoder_host_->FillThisBuffer(video_frame);
}

void IpcVideoDecoder::OnFillBufferDone(scoped_refptr<VideoFrame> video_frame) {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::OnFillBufferDone,
                          video_frame));
    return;
  }

  if (video_frame.get()) {
    --pending_requests_;
    fill_buffer_done_callback()->Run(video_frame);
    if (state_ == kFlushing && pending_reads_ == 0 && pending_requests_ == 0) {
      CHECK(flush_callback_.get());
      flush_callback_->Run();
      flush_callback_.reset();
      state_ = kPlaying;
    }

  } else {
    if (state_ == kFlushCodec) {
      // When in kFlushCodec, any errored decode, or a 0-lengthed frame,
      // is taken as a signal to stop decoding.
      state_ = kEnded;
      scoped_refptr<VideoFrame> video_frame;
      VideoFrame::CreateEmptyFrame(&video_frame);
      fill_buffer_done_callback()->Run(video_frame);
    }
  }
}

void IpcVideoDecoder::OnEmptyBufferDone(scoped_refptr<Buffer> buffer) {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::OnEmptyBufferDone,
                          buffer));
    return;
  }

  // TODO(jiesun): We haven't recycle input buffer yet.
  demuxer_stream_->Read(NewCallback(this, &IpcVideoDecoder::OnReadComplete));
  ++pending_reads_;
}

void IpcVideoDecoder::OnDeviceError() {
  host()->SetError(PIPELINE_ERROR_DECODE);
}

bool IpcVideoDecoder::ProvidesBuffer() {
  return true;
}

// static
FilterFactory* IpcVideoDecoder::CreateFactory(MessageLoop* message_loop) {
  return new FilterFactoryImpl1<IpcVideoDecoder, MessageLoop*>(message_loop);
}

// static
bool IpcVideoDecoder::IsMediaFormatSupported(const MediaFormat& format) {
  std::string mime_type;
  if (!format.GetAsString(MediaFormat::kMimeType, &mime_type) &&
      mime_type::kFFmpegVideo != mime_type)
      return false;

  // TODO(jiesun): Although we current only support H264 hardware decoding,
  // in the future, we should query GpuVideoService for capabilities.
  int codec_id;
  return format.GetAsInteger(MediaFormat::kFFmpegCodecID, &codec_id) &&
         codec_id == CODEC_ID_H264;
}

}  // namespace media
