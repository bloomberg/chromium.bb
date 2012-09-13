// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/capture_video_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "media/base/demuxer_stream.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"

using media::CopyYPlane;
using media::CopyUPlane;
using media::CopyVPlane;

CaptureVideoDecoder::CaptureVideoDecoder(
    base::MessageLoopProxy* message_loop_proxy,
    media::VideoCaptureSessionId video_stream_id,
    VideoCaptureImplManager* vc_manager,
    const media::VideoCaptureCapability& capability)
    : message_loop_proxy_(message_loop_proxy),
      vc_manager_(vc_manager),
      capability_(capability),
      natural_size_(capability.width, capability.height),
      state_(kUnInitialized),
      got_first_frame_(false),
      video_stream_id_(video_stream_id),
      capture_engine_(NULL) {
  DCHECK(vc_manager);
}

void CaptureVideoDecoder::Initialize(
    const scoped_refptr<media::DemuxerStream>& stream,
    const media::PipelineStatusCB& status_cb,
    const media::StatisticsCB& statistics_cb) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::InitializeOnDecoderThread,
                 this, stream, status_cb, statistics_cb));
}

void CaptureVideoDecoder::Read(const ReadCB& read_cb) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::ReadOnDecoderThread,
                 this, read_cb));
}

void CaptureVideoDecoder::Reset(const base::Closure& closure) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::ResetOnDecoderThread, this, closure));
}

void CaptureVideoDecoder::Stop(const base::Closure& closure) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::StopOnDecoderThread, this, closure));
}

void CaptureVideoDecoder::OnStarted(media::VideoCapture* capture) {
  NOTIMPLEMENTED();
}

void CaptureVideoDecoder::OnStopped(media::VideoCapture* capture) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::OnStoppedOnDecoderThread,
                 this, capture));
}

void CaptureVideoDecoder::OnPaused(media::VideoCapture* capture) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::OnPausedOnDecoderThread,
                 this, capture));
}

void CaptureVideoDecoder::OnError(media::VideoCapture* capture,
                                  int error_code) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::OnPausedOnDecoderThread,
                 this, capture));
}

void CaptureVideoDecoder::OnRemoved(media::VideoCapture* capture) {
  NOTIMPLEMENTED();
}

void CaptureVideoDecoder::OnBufferReady(
    media::VideoCapture* capture,
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf) {
  DCHECK(buf);
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::OnBufferReadyOnDecoderThread,
                 this, capture, buf));
}

void CaptureVideoDecoder::OnDeviceInfoReceived(
    media::VideoCapture* capture,
    const media::VideoCaptureParams& device_info) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::OnDeviceInfoReceivedOnDecoderThread,
                 this, capture, device_info));
}

CaptureVideoDecoder::~CaptureVideoDecoder() {}

void CaptureVideoDecoder::InitializeOnDecoderThread(
    const scoped_refptr<media::DemuxerStream>& /* stream */,
    const media::PipelineStatusCB& status_cb,
    const media::StatisticsCB& statistics_cb) {
  DVLOG(1) << "InitializeOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  capture_engine_ = vc_manager_->AddDevice(video_stream_id_, this);

  statistics_cb_ = statistics_cb;
  status_cb.Run(media::PIPELINE_OK);
  state_ = kNormal;
  capture_engine_->StartCapture(this, capability_);
}

void CaptureVideoDecoder::ReadOnDecoderThread(const ReadCB& read_cb) {
  DCHECK_NE(state_, kUnInitialized);
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  CHECK(read_cb_.is_null());
  read_cb_ = read_cb;
  if (state_ == kPaused || state_ == kStopped) {
    DeliverFrame(media::VideoFrame::CreateEmptyFrame());
  }
}

void CaptureVideoDecoder::ResetOnDecoderThread(const base::Closure& closure) {
  DVLOG(1) << "ResetOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (!read_cb_.is_null()) {
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateBlackFrame(natural_size_);
    DeliverFrame(video_frame);
  }
  closure.Run();
}

void CaptureVideoDecoder::StopOnDecoderThread(const base::Closure& closure) {
  DVLOG(1) << "StopOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  pending_stop_cb_ = closure;
  state_ = kStopped;

  if (!read_cb_.is_null())
    DeliverFrame(media::VideoFrame::CreateEmptyFrame());

  capture_engine_->StopCapture(this);
}

void CaptureVideoDecoder::OnStoppedOnDecoderThread(
    media::VideoCapture* capture) {
  DVLOG(1) << "OnStoppedOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (!pending_stop_cb_.is_null())
    base::ResetAndReturn(&pending_stop_cb_).Run();
  vc_manager_->RemoveDevice(video_stream_id_, this);
}

void CaptureVideoDecoder::OnPausedOnDecoderThread(
    media::VideoCapture* capture) {
  DVLOG(1) << "OnPausedOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  state_ = kPaused;
  if (!read_cb_.is_null()) {
    DeliverFrame(media::VideoFrame::CreateEmptyFrame());
  }
}

void CaptureVideoDecoder::OnDeviceInfoReceivedOnDecoderThread(
    media::VideoCapture* capture,
    const media::VideoCaptureParams& device_info) {
  DVLOG(1) << "OnDeviceInfoReceivedOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (device_info.width != natural_size_.width() ||
      device_info.height != natural_size_.height()) {
    natural_size_.SetSize(device_info.width, device_info.height);
  }
}

void CaptureVideoDecoder::OnBufferReadyOnDecoderThread(
    media::VideoCapture* capture,
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  if (read_cb_.is_null() || kNormal != state_) {
    // TODO(wjia): revisit TS adjustment when crbug.com/111672 is resolved.
    if (got_first_frame_) {
      start_time_ += buf->timestamp - last_frame_timestamp_;
    }
    last_frame_timestamp_ = buf->timestamp;
    capture->FeedBuffer(buf);
    return;
  }

  // TODO(wjia): should we always expect device to send device info before
  // any buffer, and buffers should have dimension stated in device info?
  // Or should we be flexible as in following code?
  if (buf->width != natural_size_.width() ||
      buf->height != natural_size_.height()) {
    natural_size_.SetSize(buf->width, buf->height);
  }

  // Need to rebase timestamp with zero as starting point.
  if (!got_first_frame_) {
    start_time_ = buf->timestamp;
    got_first_frame_ = true;
  }

  // Always allocate a new frame.
  //
  // TODO(scherkus): migrate this to proper buffer recycling.
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateFrame(media::VideoFrame::YV12,
                                     natural_size_, natural_size_,
                                     buf->timestamp - start_time_);

  last_frame_timestamp_ = buf->timestamp;
  uint8* buffer = buf->memory_pointer;

  // Assume YV12 format. Note that camera gives YUV and media pipeline video
  // renderer asks for YVU. The following code did the conversion.
  DCHECK_EQ(capability_.color, media::VideoCaptureCapability::kI420);
  int y_width = buf->width;
  int y_height = buf->height;
  int uv_width = buf->width / 2;
  int uv_height = buf->height / 2;  // YV12 format.
  CopyYPlane(buffer, y_width, y_height, video_frame);
  buffer += y_width * y_height;
  CopyUPlane(buffer, uv_width, uv_height, video_frame);
  buffer += uv_width * uv_height;
  CopyVPlane(buffer, uv_width, uv_height, video_frame);

  DeliverFrame(video_frame);
  capture->FeedBuffer(buf);
}

void CaptureVideoDecoder::DeliverFrame(
    const scoped_refptr<media::VideoFrame>& video_frame) {
  // Reset the callback before running to protect against reentrancy.
  base::ResetAndReturn(&read_cb_).Run(kOk, video_frame);
}
