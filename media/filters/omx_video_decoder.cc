// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/omx_video_decoder.h"

#include "base/callback.h"
#include "media/base/callback.h"
#include "media/base/factory.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_interfaces.h"
#include "media/filters/omx_video_decode_engine.h"

namespace media {

// static
FilterFactory* OmxVideoDecoder::CreateFactory() {
  return new FilterFactoryImpl1<OmxVideoDecoder, OmxVideoDecodeEngine*>(
      new OmxVideoDecodeEngine());
}

// static
bool OmxVideoDecoder::IsMediaFormatSupported(const MediaFormat& format) {
  std::string mime_type;
  if (!format.GetAsString(MediaFormat::kMimeType, &mime_type) ||
      mime_type::kFFmpegVideo != mime_type) {
    return false;
  }

  // TODO(ajwong): Find a good way to white-list formats that OpenMAX can
  // handle.
  int codec_id;
  if (format.GetAsInteger(MediaFormat::kFFmpegCodecID, &codec_id) &&
      codec_id == CODEC_ID_H264) {
    return true;
  }

  return false;
}

OmxVideoDecoder::OmxVideoDecoder(OmxVideoDecodeEngine* engine)
    : omx_engine_(engine) {
#if defined(ENABLE_EGLIMAGE)
  supports_egl_image_ = true;
#else
  supports_egl_image_ = false;
#endif
  DCHECK(omx_engine_.get());
}

OmxVideoDecoder::~OmxVideoDecoder() {
  // TODO(hclam): Make sure OmxVideoDecodeEngine is stopped.
}

void OmxVideoDecoder::Initialize(DemuxerStream* stream,
                                 FilterCallback* callback) {
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &OmxVideoDecoder::DoInitialize,
                        stream,
                        callback));
}

void OmxVideoDecoder::FillThisBuffer(scoped_refptr<VideoFrame> frame) {
  DCHECK(omx_engine_.get());
  message_loop()->PostTask(
     FROM_HERE,
     NewRunnableMethod(omx_engine_.get(),
                       &OmxVideoDecodeEngine::FillThisBuffer, frame));
}

bool OmxVideoDecoder::ProvidesBuffer() {
  if (!omx_engine_.get()) return false;
  return omx_engine_->ProvidesBuffer();
}

void OmxVideoDecoder::Stop(FilterCallback* callback) {
  omx_engine_->Stop(
     NewRunnableMethod(this,
                       &OmxVideoDecoder::StopCompleteTask, callback));
}

void OmxVideoDecoder::StopCompleteTask(FilterCallback* callback) {
  AutoCallbackRunner done_runner(callback);
}

void OmxVideoDecoder::Pause(FilterCallback* callback) {
  omx_engine_->Pause(
       NewRunnableMethod(this,
                         &OmxVideoDecoder::PauseCompleteTask, callback));
}

void OmxVideoDecoder::PauseCompleteTask(FilterCallback* callback) {
  AutoCallbackRunner done_runner(callback);
}

void OmxVideoDecoder::Flush(FilterCallback* callback) {
  omx_engine_->Flush(
       NewRunnableMethod(this,
                         &OmxVideoDecoder::FlushCompleteTask, callback));
}

void OmxVideoDecoder::FlushCompleteTask(FilterCallback* callback) {
  AutoCallbackRunner done_runner(callback);
}

void OmxVideoDecoder::Seek(base::TimeDelta time,
                           FilterCallback* callback) {
  omx_engine_->Seek(
     NewRunnableMethod(this,
                       &OmxVideoDecoder::SeekCompleteTask, callback));
}

void OmxVideoDecoder::SeekCompleteTask(FilterCallback* callback) {
  AutoCallbackRunner done_runner(callback);
}

void OmxVideoDecoder::DoInitialize(DemuxerStream* demuxer_stream,
                                   FilterCallback* callback) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Get the AVStream by querying for the provider interface.
  AVStreamProvider* av_stream_provider;
  if (!demuxer_stream->QueryInterface(&av_stream_provider)) {
    return;
  }
  AVStream* av_stream = av_stream_provider->GetAVStream();

  width_ = av_stream->codec->width;
  height_ = av_stream->codec->height;
  if (width_ > Limits::kMaxDimension ||
      height_ > Limits::kMaxDimension ||
      (width_ * height_) > Limits::kMaxCanvas) {
    return;
  }

  // Savs the demuxer stream.
  demuxer_stream_ = demuxer_stream;

  // Initialize the decode engine.
  omx_engine_->Initialize(
      message_loop(),
      av_stream,
      NewCallback(this, &OmxVideoDecoder::EmptyBufferCallback),
      NewCallback(this, &OmxVideoDecoder::FillBufferCallback),
      NewRunnableMethod(this, &OmxVideoDecoder::InitCompleteTask, callback));
}

void OmxVideoDecoder::FillBufferCallback(scoped_refptr<VideoFrame> frame) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Invoke the FillBufferDoneCallback with the frame.
  DCHECK(fill_buffer_done_callback());
  fill_buffer_done_callback()->Run(frame);
}

void OmxVideoDecoder::EmptyBufferCallback(scoped_refptr<Buffer> buffer) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Issue more demux.
  demuxer_stream_->Read(NewCallback(this, &OmxVideoDecoder::DemuxCompleteTask));
}

void OmxVideoDecoder::InitCompleteTask(FilterCallback* callback) {
  DCHECK_EQ(message_loop(), MessageLoop::current());

  // Check the status of the decode engine.
  if (omx_engine_->state() == VideoDecodeEngine::kError) {
    host()->SetError(PIPELINE_ERROR_DECODE);
  } else {
    media_format_.SetAsString(MediaFormat::kMimeType,
                              mime_type::kUncompressedVideo);
    // TODO(jiesun): recycle OmxHeadType instead of copy back.
    media_format_.SetAsInteger(MediaFormat::kSurfaceType,
        supports_egl_image_ ? VideoFrame::TYPE_EGL_IMAGE
                            : VideoFrame::TYPE_SYSTEM_MEMORY);
    media_format_.SetAsInteger(MediaFormat::kWidth, width_);
    media_format_.SetAsInteger(MediaFormat::kHeight, height_);
    VideoFrame::Format format = omx_engine_->GetSurfaceFormat();
    media_format_.SetAsInteger(MediaFormat::kSurfaceFormat,
                               static_cast<int>(format));
  }

  callback->Run();
  delete callback;
}

void OmxVideoDecoder::DemuxCompleteTask(Buffer* buffer) {
  // We simply delicate the buffer to the right message loop.
  scoped_refptr<Buffer> ref_buffer = buffer;
  DCHECK(omx_engine_.get());
  message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(omx_engine_.get(),
                        &OmxVideoDecodeEngine::EmptyThisBuffer, ref_buffer));
}

}  // namespace media
