// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_decoder.h"

#include <deque>

#include "base/task.h"
#include "media/base/callback.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/limits.h"
#include "media/base/media_format.h"
#include "media/base/video_frame.h"

using media::DemuxerStream;
using media::FilterCallback;
using media::FilterStatusCB;
using media::kNoTimestamp;
using media::Limits;
using media::MediaFormat;
using media::PIPELINE_OK;
using media::StatisticsCallback;
using media::VideoDecoder;
using media::VideoFrame;

RTCVideoDecoder::RTCVideoDecoder(MessageLoop* message_loop,
                                 const std::string& url)
    : message_loop_(message_loop),
      width_(176),
      height_(144),
      url_(url),
      state_(kUnInitialized) {
}

RTCVideoDecoder::~RTCVideoDecoder() {
}

void RTCVideoDecoder::Initialize(DemuxerStream* demuxer_stream,
                                 FilterCallback* filter_callback,
                                 StatisticsCallback* stat_callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &RTCVideoDecoder::Initialize,
                          make_scoped_refptr(demuxer_stream),
                          filter_callback, stat_callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  lock_.Acquire();
  frame_queue_available_.clear();
  lock_.Release();
  media_format_.SetAsInteger(MediaFormat::kWidth, width_);
  media_format_.SetAsInteger(MediaFormat::kHeight, height_);
  media_format_.SetAsInteger(MediaFormat::kSurfaceType,
                             static_cast<int>(VideoFrame::YV12));
  media_format_.SetAsInteger(MediaFormat::kSurfaceFormat,
                             static_cast<int>(VideoFrame::TYPE_SYSTEM_MEMORY));

  state_ = kNormal;

  filter_callback->Run();
  delete filter_callback;

  // TODO(acolwell): Implement stats.
  delete stat_callback;
}

void RTCVideoDecoder::Play(FilterCallback* callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                             NewRunnableMethod(this,
                                               &RTCVideoDecoder::Play,
                                               callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  VideoDecoder::Play(callback);
}

void RTCVideoDecoder::Pause(FilterCallback* callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                            NewRunnableMethod(this,
                                              &RTCVideoDecoder::Pause,
                                              callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  state_ = kPaused;

  VideoDecoder::Pause(callback);
}

void RTCVideoDecoder::Stop(FilterCallback* callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                            NewRunnableMethod(this,
                                              &RTCVideoDecoder::Stop,
                                              callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  state_ = kStopped;

  VideoDecoder::Stop(callback);
}

void RTCVideoDecoder::Seek(base::TimeDelta time, const FilterStatusCB& cb) {
  if (MessageLoop::current() != message_loop_) {
     message_loop_->PostTask(FROM_HERE,
                             NewRunnableMethod(this, &RTCVideoDecoder::Seek,
                                               time, cb));
     return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  state_ = kSeeking;
  // Create output buffer pool and pass the frames to renderer
  // so that the renderer can complete the seeking
  for (size_t i = 0; i < Limits::kMaxVideoFrames; ++i) {
    scoped_refptr<VideoFrame> video_frame;
    VideoFrame::CreateFrame(VideoFrame::YV12,
                            width_,
                            height_,
                            kNoTimestamp,
                            kNoTimestamp,
                            &video_frame);
    if (!video_frame.get()) {
      break;
    }

    // Create black frame
    const uint8 kBlackY = 0x00;
    const uint8 kBlackUV = 0x80;
    // Fill the Y plane.
    uint8* y_plane = video_frame->data(VideoFrame::kYPlane);
    for (size_t i = 0; i < height_; ++i) {
      memset(y_plane, kBlackY, width_);
      y_plane += video_frame->stride(VideoFrame::kYPlane);
    }
    // Fill the U and V planes.
    uint8* u_plane = video_frame->data(VideoFrame::kUPlane);
    uint8* v_plane = video_frame->data(VideoFrame::kVPlane);
    for (size_t i = 0; i < (height_ / 2); ++i) {
      memset(u_plane, kBlackUV, width_ / 2);
      memset(v_plane, kBlackUV, width_ / 2);
      u_plane += video_frame->stride(VideoFrame::kUPlane);
      v_plane += video_frame->stride(VideoFrame::kVPlane);
    }

    VideoFrameReady(video_frame);
  }

  state_ = kNormal;

  cb.Run(PIPELINE_OK);
}

const MediaFormat& RTCVideoDecoder::media_format() {
  return media_format_;
}

void RTCVideoDecoder::ProduceVideoFrame(
    scoped_refptr<VideoFrame> video_frame) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &RTCVideoDecoder::ProduceVideoFrame, video_frame));
    return;
  }
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  base::AutoLock auto_lock(lock_);
  frame_queue_available_.push_back(video_frame);
}

bool RTCVideoDecoder::ProvidesBuffer() {
  return true;
}

bool RTCVideoDecoder::SetSize(int width, int height, int reserved) {
  width_ = width;
  height_ = height;

  media_format_.SetAsInteger(MediaFormat::kWidth, width_);
  media_format_.SetAsInteger(MediaFormat::kHeight, height_);
  host()->SetVideoSize(width_, height_);
  return true;
}

bool RTCVideoDecoder::RenderFrame(const cricket::VideoFrame* frame) {
  DCHECK(frame);

  if (state_ != kNormal)
    return true;

  // This is called from another thread
  scoped_refptr<VideoFrame> video_frame;
  {
    base::AutoLock auto_lock(lock_);
    if (frame_queue_available_.size() == 0) {
      return true;
    }
    video_frame = frame_queue_available_.front();
    frame_queue_available_.pop_front();
  }

  // Check if there's a size change
  if (video_frame->width() != width_ || video_frame->height() != height_) {
    video_frame.release();
    // Allocate new buffer based on the new size
    VideoFrame::CreateFrame(VideoFrame::YV12,
                            width_,
                            height_,
                            kNoTimestamp,
                            kNoTimestamp,
                            &video_frame);
  }

  video_frame->SetTimestamp(host()->GetTime());
  video_frame->SetDuration(base::TimeDelta::FromMilliseconds(30));

  uint8* y_plane = video_frame->data(VideoFrame::kYPlane);
  const uint8* y_plane_src = frame->GetYPlane();
  for (size_t row = 0; row < video_frame->height(); ++row) {
    memcpy(y_plane, y_plane_src, frame->GetYPitch());
    y_plane += video_frame->stride(VideoFrame::kYPlane);
    y_plane_src += frame->GetYPitch();
  }
  uint8* u_plane = video_frame->data(VideoFrame::kUPlane);
  const uint8* u_plane_src = frame->GetUPlane();
  for (size_t row = 0; row < video_frame->height(); row += 2) {
    memcpy(u_plane, u_plane_src, frame->GetUPitch());
    u_plane += video_frame->stride(VideoFrame::kUPlane);
    u_plane_src += frame->GetUPitch();
  }
  uint8* v_plane = video_frame->data(VideoFrame::kVPlane);
  const uint8* v_plane_src = frame->GetVPlane();
  for (size_t row = 0; row < video_frame->height(); row += 2) {
    memcpy(v_plane, v_plane_src, frame->GetVPitch());
    v_plane += video_frame->stride(VideoFrame::kVPlane);
    v_plane_src += frame->GetVPitch();
  }

  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &RTCVideoDecoder::VideoFrameReady,
                          video_frame));
  } else {
    VideoFrameReady(video_frame);
  }

  return true;
}
