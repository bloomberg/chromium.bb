// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/capture_video_decoder.h"

#include "base/bind.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "media/base/filter_host.h"
#include "media/base/limits.h"
#include "media/base/video_util.h"

using media::CopyYPlane;
using media::CopyUPlane;
using media::CopyVPlane;

CaptureVideoDecoder::CaptureVideoDecoder(
    base::MessageLoopProxy* message_loop_proxy,
    media::VideoCaptureSessionId video_stream_id,
    VideoCaptureImplManager* vc_manager,
    const media::VideoCapture::VideoCaptureCapability& capability)
    : message_loop_proxy_(message_loop_proxy),
      vc_manager_(vc_manager),
      capability_(capability),
      natural_size_(capability.width, capability.height),
      state_(kUnInitialized),
      video_stream_id_(video_stream_id),
      capture_engine_(NULL) {
  DCHECK(vc_manager);
}

CaptureVideoDecoder::~CaptureVideoDecoder() {}

void CaptureVideoDecoder::Initialize(
    media::DemuxerStream* demuxer_stream,
    const base::Closure& filter_callback,
    const media::StatisticsCallback& stat_callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::InitializeOnDecoderThread,
                 this, make_scoped_refptr(demuxer_stream),
                 filter_callback, stat_callback));
}

void CaptureVideoDecoder::Read(const ReadCB& callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::ReadOnDecoderThread,
                 this, callback));
}

const gfx::Size& CaptureVideoDecoder::natural_size() {
  return natural_size_;
}

void CaptureVideoDecoder::Play(const base::Closure& callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::PlayOnDecoderThread,
                 this, callback));
}

void CaptureVideoDecoder::Pause(const base::Closure& callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::PauseOnDecoderThread,
                 this, callback));
}

void CaptureVideoDecoder::Flush(const base::Closure& callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::FlushOnDecoderThread,
                 this, callback));
}

void CaptureVideoDecoder::Stop(const base::Closure& callback) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::StopOnDecoderThread,
                 this, callback));
}

void CaptureVideoDecoder::Seek(base::TimeDelta time,
                               const media::FilterStatusCB& cb) {
  message_loop_proxy_->PostTask(
      FROM_HERE,
      base::Bind(&CaptureVideoDecoder::SeekOnDecoderThread,
                 this, time, cb));
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
  NOTIMPLEMENTED();
}

void CaptureVideoDecoder::OnError(media::VideoCapture* capture,
                                  int error_code) {
  NOTIMPLEMENTED();
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

void CaptureVideoDecoder::InitializeOnDecoderThread(
    media::DemuxerStream* demuxer_stream,
    const base::Closure& filter_callback,
    const media::StatisticsCallback& stat_callback) {
  DVLOG(1) << "InitializeOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  capture_engine_ = vc_manager_->AddDevice(video_stream_id_, this);

  statistics_callback_ = stat_callback;
  filter_callback.Run();
  state_ = kNormal;
}

void CaptureVideoDecoder::ReadOnDecoderThread(const ReadCB& callback) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  CHECK(read_cb_.is_null());
  read_cb_ = callback;
}

void CaptureVideoDecoder::PlayOnDecoderThread(const base::Closure& callback) {
  DVLOG(1) << "PlayOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  callback.Run();
}

void CaptureVideoDecoder::PauseOnDecoderThread(const base::Closure& callback) {
  DVLOG(1) << "PauseOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  state_ = kPaused;
  media::VideoDecoder::Pause(callback);
}

void CaptureVideoDecoder::FlushOnDecoderThread(const base::Closure& callback) {
  DVLOG(1) << "FlushOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (!read_cb_.is_null()) {
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateBlackFrame(natural_size_.width(),
                                            natural_size_.height());
    DeliverFrame(video_frame);
  }
  media::VideoDecoder::Flush(callback);
}

void CaptureVideoDecoder::StopOnDecoderThread(const base::Closure& callback) {
  DVLOG(1) << "StopOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  pending_stop_cb_ = callback;
  state_ = kStopped;
  capture_engine_->StopCapture(this);
}

void CaptureVideoDecoder::SeekOnDecoderThread(base::TimeDelta time,
                                              const media::FilterStatusCB& cb) {
  DVLOG(1) << "SeekOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  cb.Run(media::PIPELINE_OK);
  state_ = kNormal;
  capture_engine_->StartCapture(this, capability_);
}

void CaptureVideoDecoder::OnStoppedOnDecoderThread(
    media::VideoCapture* capture) {
  DVLOG(1) << "OnStoppedOnDecoderThread";
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (!pending_stop_cb_.is_null())
    media::ResetAndRunCB(&pending_stop_cb_);
  vc_manager_->RemoveDevice(video_stream_id_, this);
}

void CaptureVideoDecoder::OnDeviceInfoReceivedOnDecoderThread(
    media::VideoCapture* capture,
    const media::VideoCaptureParams& device_info) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());
  if (device_info.width != natural_size_.width() ||
      device_info.height != natural_size_.height()) {
    natural_size_.SetSize(device_info.width, device_info.height);
    host()->SetNaturalVideoSize(natural_size_);
  }
}

void CaptureVideoDecoder::OnBufferReadyOnDecoderThread(
    media::VideoCapture* capture,
    scoped_refptr<media::VideoCapture::VideoFrameBuffer> buf) {
  DCHECK(message_loop_proxy_->BelongsToCurrentThread());

  if (read_cb_.is_null() || kNormal != state_) {
    capture->FeedBuffer(buf);
    return;
  }

  // TODO(wjia): should we always expect device to send device info before
  // any buffer, and buffers should have dimension stated in device info?
  // Or should we be flexible as in following code?
  if (buf->width != natural_size_.width() ||
      buf->height != natural_size_.height()) {
    natural_size_.SetSize(buf->width, buf->height);
    host()->SetNaturalVideoSize(natural_size_);
  }

  // Always allocate a new frame.
  //
  // TODO(scherkus): migrate this to proper buffer recycling.
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateFrame(media::VideoFrame::YV12,
                                     natural_size_.width(),
                                     natural_size_.height(),
                                     buf->timestamp - start_time_,
                                     base::TimeDelta::FromMilliseconds(33));

  uint8* buffer = buf->memory_pointer;

  // Assume YV12 format. Note that camera gives YUV and media pipeline video
  // renderer asks for YVU. The following code did the conversion.
  DCHECK_EQ(capability_.raw_type, media::VideoFrame::I420);
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
  ReadCB read_cb = read_cb_;
  read_cb_.Reset();
  read_cb.Run(video_frame);
}
