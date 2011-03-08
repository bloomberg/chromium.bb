// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/ipc_video_decoder.h"

#include "base/task.h"
#include "chrome/renderer/ggl/ggl.h"
#include "content/common/child_process.h"
#include "media/base/callback.h"
#include "media/base/filters.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/base/media_format.h"
#include "media/base/video_frame.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/ffmpeg/ffmpeg_util.h"
#include "media/filters/ffmpeg_interfaces.h"
#include "media/video/video_decode_engine.h"

IpcVideoDecoder::IpcVideoDecoder(MessageLoop* message_loop,
                                 ggl::Context* ggl_context)
    : decode_context_message_loop_(message_loop),
      ggl_context_(ggl_context) {
}

IpcVideoDecoder::~IpcVideoDecoder() {
}

void IpcVideoDecoder::Initialize(media::DemuxerStream* demuxer_stream,
                                 media::FilterCallback* callback,
                                 media::StatisticsCallback* statsCallback) {
  // It doesn't matter which thread we perform initialization because
  // all this method does is create objects and delegate the initialize
  // messsage.

  DCHECK(!demuxer_stream_);
  demuxer_stream_ = demuxer_stream;
  initialize_callback_.reset(callback);
  statistics_callback_.reset(statsCallback);

  // We require bit stream converter for hardware decoder.
  demuxer_stream->EnableBitstreamConverter();

  // Get the AVStream by querying for the provider interface.
  media::AVStreamProvider* av_stream_provider;
  if (!demuxer_stream->QueryInterface(&av_stream_provider)) {
    media::VideoCodecInfo info = {0};
    OnInitializeComplete(info);
    return;
  }

  AVStream* av_stream = av_stream_provider->GetAVStream();

  int width = av_stream->codec->width;
  int height = av_stream->codec->height;
  if (width > media::Limits::kMaxDimension ||
      height > media::Limits::kMaxDimension ||
      (width * height) > media::Limits::kMaxCanvas) {
    media::VideoCodecInfo info = {0};
    OnInitializeComplete(info);
    return;
  }

  // Create a video decode context that assocates with the graphics
  // context.
  decode_context_.reset(
      ggl::CreateVideoDecodeContext(
          ggl_context_, decode_context_message_loop_, true));

  // Create a hardware video decoder handle.
  decode_engine_.reset(ggl::CreateVideoDecodeEngine(ggl_context_));

  // Initialize hardware decoder.
  media::VideoCodecConfig param;
  memset(&param, 0, sizeof(param));
  param.width = width;
  param.height = height;

  // VideoDecodeEngine will perform initialization on the message loop
  // given to it so it doesn't matter on which thread we are calling this.
  decode_engine_->Initialize(ChildProcess::current()->io_message_loop(), this,
                             decode_context_.get(), param);
}

const media::MediaFormat& IpcVideoDecoder::media_format() {
  return media_format_;
}

void IpcVideoDecoder::Stop(media::FilterCallback* callback) {
  stop_callback_.reset(callback);
  decode_engine_->Uninitialize();
}

void IpcVideoDecoder::Pause(media::FilterCallback* callback) {
  // TODO(hclam): It looks like that pause is not necessary so implement this
  // later.
  callback->Run();
  delete callback;
}

void IpcVideoDecoder::Flush(media::FilterCallback* callback) {
  flush_callback_.reset(callback);
  decode_engine_->Flush();
}

void IpcVideoDecoder::Seek(base::TimeDelta time,
                           media::FilterCallback* callback) {
  seek_callback_.reset(callback);
  decode_engine_->Seek();
}

void IpcVideoDecoder::OnInitializeComplete(const media::VideoCodecInfo& info) {
  DCHECK_EQ(ChildProcess::current()->io_message_loop(), MessageLoop::current());

  if (info.success) {
    media_format_.SetAsString(media::MediaFormat::kMimeType,
                              media::mime_type::kUncompressedVideo);
    media_format_.SetAsInteger(media::MediaFormat::kSurfaceType,
                               media::VideoFrame::TYPE_GL_TEXTURE);
    media_format_.SetAsInteger(media::MediaFormat::kSurfaceFormat,
                               info.stream_info.surface_format);
    media_format_.SetAsInteger(media::MediaFormat::kWidth,
                               info.stream_info.surface_width);
    media_format_.SetAsInteger(media::MediaFormat::kHeight,
                               info.stream_info.surface_height);
    media_format_.SetAsInteger(
        media::MediaFormat::kSurfaceType,
        static_cast<int>(media::VideoFrame::TYPE_GL_TEXTURE));
  } else {
    LOG(ERROR) << "IpcVideoDecoder initialization failed!";
    host()->SetError(media::PIPELINE_ERROR_DECODE);
  }

  initialize_callback_->Run();
  initialize_callback_.reset();
}

void IpcVideoDecoder::OnUninitializeComplete() {
  DCHECK_EQ(ChildProcess::current()->io_message_loop(), MessageLoop::current());

  // After the decode engine is uninitialized we are safe to destroy the decode
  // context. The task will add a refcount to this object so don't need to worry
  // about objects lifetime.
  decode_context_->Destroy(
      NewRunnableMethod(this, &IpcVideoDecoder::OnDestroyComplete));

  // We don't need to wait for destruction of decode context to complete because
  // it can happen asynchronously. This object and decode context will live
  // until the destruction task is called.
  stop_callback_->Run();
  stop_callback_.reset();
}

void IpcVideoDecoder::OnFlushComplete() {
  DCHECK_EQ(ChildProcess::current()->io_message_loop(), MessageLoop::current());
  flush_callback_->Run();
  flush_callback_.reset();
}

void IpcVideoDecoder::OnSeekComplete() {
  DCHECK_EQ(ChildProcess::current()->io_message_loop(), MessageLoop::current());
  seek_callback_->Run();
  seek_callback_.reset();
}

void IpcVideoDecoder::OnError() {
  DCHECK_EQ(ChildProcess::current()->io_message_loop(), MessageLoop::current());
  host()->SetError(media::PIPELINE_ERROR_DECODE);
}

// This methid is called by Demuxer after a demuxed packet is produced.
void IpcVideoDecoder::OnReadComplete(media::Buffer* buffer) {
  decode_engine_->ConsumeVideoSample(buffer);
}

void IpcVideoDecoder::OnDestroyComplete() {
  // We don't need to do anything in this method. Destruction of objects will
  // occur as soon as refcount goes to 0.
}

// This method is called by VideoRenderer. We delegate the method call to
// VideoDecodeEngine.
void IpcVideoDecoder::ProduceVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame) {
  decode_engine_->ProduceVideoFrame(video_frame);
}

bool IpcVideoDecoder::ProvidesBuffer() {
  return true;
}

// This method is called by VideoDecodeEngine that a video frame is produced.
// This is then passed to VideoRenderer.
void IpcVideoDecoder::ConsumeVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    const media::PipelineStatistics& statistics) {
  DCHECK(video_frame);
  statistics_callback_->Run(statistics);

  VideoFrameReady(video_frame);
}

// This method is called by VideoDecodeEngine to request a video frame. The
// request is passed to demuxer.
void IpcVideoDecoder::ProduceVideoSample(scoped_refptr<media::Buffer> buffer) {
  demuxer_stream_->Read(NewCallback(this, &IpcVideoDecoder::OnReadComplete));
}
