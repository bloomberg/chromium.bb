// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/capture_video_decoder.h"

#include "content/renderer/media/video_capture_impl_manager.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/base/media_format.h"

CaptureVideoDecoder::CaptureVideoDecoder(
    base::MessageLoopProxy* message_loop_proxy,
    media::VideoCaptureSessionId video_stream_id,
    VideoCaptureImplManager* vc_manager,
    const media::VideoCapture::VideoCaptureCapability& capability)
    : message_loop_proxy_(message_loop_proxy),
      vc_manager_(vc_manager),
      capability_(capability),
      state_(kUnInitialized),
      pending_stop_cb_(NULL),
      video_stream_id_(video_stream_id),
      capture_engine_(NULL) {
  DCHECK(vc_manager);
}

CaptureVideoDecoder::~CaptureVideoDecoder() {}

void CaptureVideoDecoder::Initialize(media::DemuxerStream* demuxer_stream,
                                     media::FilterCallback* filter_callback,
                                     media::StatisticsCallback* stat_callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &CaptureVideoDecoder::InitializeOnDecoderThread,
                        make_scoped_refptr(demuxer_stream),
                        filter_callback, stat_callback));
}

const media::MediaFormat& CaptureVideoDecoder::media_format() {
  return media_format_;
}

void CaptureVideoDecoder::ProduceVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(
          this,
          &CaptureVideoDecoder::ProduceVideoFrameOnDecoderThread, video_frame));
}

bool CaptureVideoDecoder::ProvidesBuffer() {
  return true;
}

void CaptureVideoDecoder::Play(media::FilterCallback* callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &CaptureVideoDecoder::PlayOnDecoderThread,
                        callback));
}

void CaptureVideoDecoder::Pause(media::FilterCallback* callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &CaptureVideoDecoder::PauseOnDecoderThread,
                        callback));
}

void CaptureVideoDecoder::Stop(media::FilterCallback* callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &CaptureVideoDecoder::StopOnDecoderThread,
                        callback));
}

void CaptureVideoDecoder::Seek(base::TimeDelta time,
                               const media::FilterStatusCB& cb) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &CaptureVideoDecoder::SeekOnDecoderThread,
                        time,
                        cb));
}

void CaptureVideoDecoder::OnStarted(media::VideoCapture* capture) {
  NOTIMPLEMENTED();
}

void CaptureVideoDecoder::OnStopped(media::VideoCapture* capture) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &CaptureVideoDecoder::OnStoppedOnDecoderThread,
                        capture));
}

void CaptureVideoDecoder::OnPaused(media::VideoCapture* capture) {
  NOTIMPLEMENTED();
}

void CaptureVideoDecoder::OnError(media::VideoCapture* capture,
                                  int error_code) {
  NOTIMPLEMENTED();
}

void CaptureVideoDecoder::OnBufferReady(
    media::VideoCapture* capture,
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf) {
  DCHECK(buf);
  message_loop_proxy_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &CaptureVideoDecoder::OnBufferReadyOnDecoderThread,
                        capture,
                        buf));
}

void CaptureVideoDecoder::OnDeviceInfoReceived(
    media::VideoCapture* capture,
    const media::VideoCaptureParams& device_info) {
  NOTIMPLEMENTED();
}

void CaptureVideoDecoder::InitializeOnDecoderThread(
    media::DemuxerStream* demuxer_stream,
    media::FilterCallback* filter_callback,
    media::StatisticsCallback* stat_callback) {
  VLOG(1) << "InitializeOnDecoderThread.";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  capture_engine_ = vc_manager_->AddDevice(video_stream_id_, this);

  available_frames_.clear();
  media_format_.SetAsInteger(media::MediaFormat::kWidth, capability_.width);
  media_format_.SetAsInteger(media::MediaFormat::kHeight, capability_.height);
  media_format_.SetAsInteger(
      media::MediaFormat::kSurfaceFormat,
      static_cast<int>(media::VideoFrame::YV12));

  statistics_callback_.reset(stat_callback);
  filter_callback->Run();
  delete filter_callback;
  state_ = kNormal;
}

void CaptureVideoDecoder::ProduceVideoFrameOnDecoderThread(
    scoped_refptr<media::VideoFrame> video_frame) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  available_frames_.push_back(video_frame);
}

void CaptureVideoDecoder::PlayOnDecoderThread(media::FilterCallback* callback) {
  VLOG(1) << "PlayOnDecoderThread.";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  callback->Run();
  delete callback;
}

void CaptureVideoDecoder::PauseOnDecoderThread(
    media::FilterCallback* callback) {
  VLOG(1) << "PauseOnDecoderThread.";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  state_ = kPaused;
  media::VideoDecoder::Pause(callback);
}

void CaptureVideoDecoder::StopOnDecoderThread(media::FilterCallback* callback) {
  VLOG(1) << "StopOnDecoderThread.";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  pending_stop_cb_ = callback;
  state_ = kStopped;
  capture_engine_->StopCapture(this);
}

void CaptureVideoDecoder::SeekOnDecoderThread(base::TimeDelta time,
                                              const media::FilterStatusCB& cb) {
  VLOG(1) << "SeekOnDecoderThread.";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  state_ = kSeeking;
  // Create output buffer pool and pass the frames to renderer
  // so that the renderer can complete the seeking
  for (size_t i = 0; i < media::Limits::kMaxVideoFrames; ++i) {
    VideoFrameReady(media::VideoFrame::CreateBlackFrame(capability_.width,
                                                        capability_.height));
  }

  cb.Run(media::PIPELINE_OK);
  state_ = kNormal;
  capture_engine_->StartCapture(this, capability_);
}

void CaptureVideoDecoder::OnStoppedOnDecoderThread(
    media::VideoCapture* capture) {
  VLOG(1) << "OnStoppedOnDecoderThread.";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (pending_stop_cb_) {
    pending_stop_cb_->Run();
    delete pending_stop_cb_;
    pending_stop_cb_ = NULL;
  }
  vc_manager_->RemoveDevice(video_stream_id_, this);
}

void CaptureVideoDecoder::OnBufferReadyOnDecoderThread(
    media::VideoCapture* capture,
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  if (available_frames_.size() == 0 || kNormal != state_) {
    capture->FeedBuffer(buf);
    return;
  }

  scoped_refptr<media::VideoFrame> video_frame = available_frames_.front();
  available_frames_.pop_front();

  if (buf->width != capability_.width || buf->height != capability_.height) {
    capability_.width = buf->width;
    capability_.height = buf->height;
    media_format_.SetAsInteger(media::MediaFormat::kWidth, capability_.width);
    media_format_.SetAsInteger(media::MediaFormat::kHeight, capability_.height);
    host()->SetVideoSize(capability_.width, capability_.height);
  }

  // Check if there's a size change.
  if (static_cast<int>(video_frame->width()) != capability_.width ||
      static_cast<int>(video_frame->height()) != capability_.height) {
    // Allocate new buffer based on the new size.
    video_frame = media::VideoFrame::CreateFrame(media::VideoFrame::YV12,
                                                 capability_.width,
                                                 capability_.height,
                                                 media::kNoTimestamp,
                                                 media::kNoTimestamp);
  }

  video_frame->SetTimestamp(buf->timestamp - start_time_);
  video_frame->SetDuration(base::TimeDelta::FromMilliseconds(33));

  uint8* buffer = buf->memory_pointer;

  // TODO(wjia): de-duplicating pixel date copying code.
  uint8* y_plane = video_frame->data(media::VideoFrame::kYPlane);
  for (size_t row = 0; row < video_frame->height(); ++row) {
    memcpy(y_plane, buffer, capability_.width);
    y_plane += video_frame->stride(media::VideoFrame::kYPlane);
    buffer += capability_.width;
  }
  size_t uv_width = capability_.width / 2;
  uint8* u_plane = video_frame->data(media::VideoFrame::kUPlane);
  for (size_t row = 0; row < video_frame->height(); row += 2) {
    memcpy(u_plane, buffer, uv_width);
    u_plane += video_frame->stride(media::VideoFrame::kUPlane);
    buffer += uv_width;
  }
  uint8* v_plane = video_frame->data(media::VideoFrame::kVPlane);
  for (size_t row = 0; row < video_frame->height(); row += 2) {
    memcpy(v_plane, buffer, uv_width);
    v_plane += video_frame->stride(media::VideoFrame::kVPlane);
    buffer += uv_width;
  }

  VideoFrameReady(video_frame);
  capture->FeedBuffer(buf);
}
