// Copyright (c) 2010 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/media/ipc_video_decoder.h"

#include <GLES2/gl2.h>

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

IpcVideoDecoder::IpcVideoDecoder(MessageLoop* message_loop,
                                 ggl::Context* ggl_context)
    : width_(0),
      height_(0),
      state_(kUnInitialized),
      pending_reads_(0),
      pending_requests_(0),
      renderer_thread_message_loop_(message_loop),
      ggl_context_(ggl_context) {
}

IpcVideoDecoder::~IpcVideoDecoder() {
}

void IpcVideoDecoder::Initialize(media::DemuxerStream* demuxer_stream,
                                 media::FilterCallback* callback) {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::Initialize,
                          demuxer_stream,
                          callback));
    return;
  }

  DCHECK(!demuxer_stream_);
  demuxer_stream_ = demuxer_stream;
  initialize_callback_.reset(callback);

  // We require bit stream converter for openmax hardware decoder.
  // TODO(hclam): This is a wrong place to initialize the demuxer stream's
  // bitstream converter.
  demuxer_stream->EnableBitstreamConverter();

  // Get the AVStream by querying for the provider interface.
  media::AVStreamProvider* av_stream_provider;
  if (!demuxer_stream->QueryInterface(&av_stream_provider)) {
    GpuVideoDecoderInitDoneParam param;
    OnInitializeDone(false, param);
    return;
  }

  AVStream* av_stream = av_stream_provider->GetAVStream();
  width_ = av_stream->codec->width;
  height_ = av_stream->codec->height;

  // Switch GL context.
  bool ret = ggl::MakeCurrent(ggl_context_);
  DCHECK(ret) << "Failed to switch GL context";

  // Generate textures to be used by the hardware video decoder in the GPU
  // process.
  // TODO(hclam): Allocation of textures should be done based on the request
  // of the GPU process.
  GLuint texture;
  glGenTextures(1, &texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, NULL);
  texture_ = texture;

  // Create a hardware video decoder handle for IPC communication.
  gpu_video_decoder_host_ = ggl::CreateVideoDecoder(ggl_context_);

  // Initialize hardware decoder.
  GpuVideoDecoderInitParam param = {0};
  param.width = width_;
  param.height = height_;
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

  media::AutoCallbackRunner done_runner(initialize_callback_.release());

  if (success) {
    media_format_.SetAsString(media::MediaFormat::kMimeType,
                              media::mime_type::kUncompressedVideo);
    media_format_.SetAsInteger(media::MediaFormat::kWidth, width_);
    media_format_.SetAsInteger(media::MediaFormat::kHeight, height_);
    media_format_.SetAsInteger(media::MediaFormat::kSurfaceType,
                               static_cast<int>(param.surface_type));
    media_format_.SetAsInteger(media::MediaFormat::kSurfaceFormat,
                               static_cast<int>(param.format));
    state_ = kPlaying;
  } else {
    LOG(ERROR) << "IpcVideoDecoder initialization failed!";
    host()->SetError(media::PIPELINE_ERROR_DECODE);
  }
}

void IpcVideoDecoder::Stop(media::FilterCallback* callback) {
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

  media::AutoCallbackRunner done_runner(stop_callback_.release());

  state_ = kStopped;
}

void IpcVideoDecoder::Pause(media::FilterCallback* callback) {
  Flush(callback);  // TODO(jiesun): move this to flush().
}

void IpcVideoDecoder::Flush(media::FilterCallback* callback) {
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

void IpcVideoDecoder::Seek(base::TimeDelta time,
                           media::FilterCallback* callback) {
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

void IpcVideoDecoder::OnSeekComplete(media::FilterCallback* callback) {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::OnSeekComplete,
                          callback));
    return;
  }

  media::AutoCallbackRunner done_runner(callback);

  state_ = kPlaying;

  for (int i = 0; i < 20; ++i) {
    demuxer_stream_->Read(
        NewCallback(this,
                    &IpcVideoDecoder::OnReadComplete));
    ++pending_reads_;
  }
}

void IpcVideoDecoder::OnReadComplete(media::Buffer* buffer) {
  scoped_refptr<media::Buffer> buffer_ref = buffer;
  ReadCompleteTask(buffer_ref);
}

void IpcVideoDecoder::ReadCompleteTask(
    scoped_refptr<media::Buffer> buffer) {
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

void IpcVideoDecoder::ProduceVideoFrame(scoped_refptr<VideoFrame> video_frame) {
  if (MessageLoop::current() != renderer_thread_message_loop_) {
    renderer_thread_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &IpcVideoDecoder::ProduceVideoFrame,
                          video_frame));
    return;
  }

  // Synchronized flushing before stop should prevent this.
  DCHECK_NE(state_, kStopped);

  // Notify decode engine the available of new frame.
  ++pending_requests_;

  VideoFrame::GlTexture textures[3] = { texture_, 0, 0 };
  scoped_refptr<VideoFrame> frame;
  media::VideoFrame::CreateFrameGlTexture(
      media::VideoFrame::RGBA, width_, height_, textures,
      base::TimeDelta(), base::TimeDelta(), &frame);
  gpu_video_decoder_host_->FillThisBuffer(frame);
}

void IpcVideoDecoder::OnFillBufferDone(
    scoped_refptr<media::VideoFrame> video_frame) {
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
    VideoFrameReady(video_frame);
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
      VideoFrameReady(video_frame);
    }
  }
}

void IpcVideoDecoder::OnEmptyBufferDone(scoped_refptr<media::Buffer> buffer) {
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
  host()->SetError(media::PIPELINE_ERROR_DECODE);
}

bool IpcVideoDecoder::ProvidesBuffer() {
  return true;
}

// static
media::FilterFactory* IpcVideoDecoder::CreateFactory(
    MessageLoop* message_loop, ggl::Context* ggl_context) {
  return new media::FilterFactoryImpl2<IpcVideoDecoder,
      MessageLoop*,
      ggl::Context*>(
          message_loop, ggl_context);
}

// static
bool IpcVideoDecoder::IsMediaFormatSupported(const media::MediaFormat& format) {
  std::string mime_type;
  if (!format.GetAsString(media::MediaFormat::kMimeType, &mime_type) &&
      media::mime_type::kFFmpegVideo != mime_type)
      return false;

  // TODO(jiesun): Although we current only support H264 hardware decoding,
  // in the future, we should query GpuVideoService for capabilities.
  int codec_id;
  return format.GetAsInteger(media::MediaFormat::kFFmpegCodecID, &codec_id) &&
      codec_id == CODEC_ID_H264;
}
