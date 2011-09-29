// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_decoder.h"

#include <deque>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "media/base/demuxer.h"
#include "media/base/filter_host.h"
#include "media/base/filters.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/libjingle/source/talk/session/phone/videoframe.h"

using media::CopyUPlane;
using media::CopyVPlane;
using media::CopyYPlane;
using media::DemuxerStream;
using media::FilterStatusCB;
using media::kNoTimestamp;
using media::Limits;
using media::PIPELINE_OK;
using media::StatisticsCallback;
using media::VideoDecoder;
using media::VideoFrame;

RTCVideoDecoder::RTCVideoDecoder(MessageLoop* message_loop,
                                 const std::string& url)
    : message_loop_(message_loop),
      visible_size_(176, 144),
      url_(url),
      state_(kUnInitialized) {
}

RTCVideoDecoder::~RTCVideoDecoder() {}

void RTCVideoDecoder::Initialize(DemuxerStream* demuxer_stream,
                                 const base::Closure& filter_callback,
                                 const StatisticsCallback& stat_callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RTCVideoDecoder::Initialize, this,
                   make_scoped_refptr(demuxer_stream),
                   filter_callback, stat_callback));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  lock_.Acquire();
  frame_queue_available_.clear();
  lock_.Release();

  state_ = kNormal;

  filter_callback.Run();

  // TODO(acolwell): Implement stats.
}

void RTCVideoDecoder::Play(const base::Closure& callback) {
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

void RTCVideoDecoder::Pause(const base::Closure& callback) {
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

void RTCVideoDecoder::Stop(const base::Closure& callback) {
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
  // so that the renderer can complete the seeking.
  for (size_t i = 0; i < Limits::kMaxVideoFrames; ++i) {
    VideoFrameReady(VideoFrame::CreateBlackFrame(
        visible_size_.width(), visible_size_.height()));
  }

  state_ = kNormal;

  cb.Run(PIPELINE_OK);
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

gfx::Size RTCVideoDecoder::natural_size() {
  // TODO(vrk): Return natural size when aspect ratio support is implemented.
  return visible_size_;
}

bool RTCVideoDecoder::SetSize(int width, int height, int reserved) {
  visible_size_.SetSize(width, height);

  // TODO(vrk): Provide natural size when aspect ratio support is implemented.
  host()->SetNaturalVideoSize(visible_size_);
  return true;
}

bool RTCVideoDecoder::RenderFrame(const cricket::VideoFrame* frame) {
  DCHECK(frame);

  if (state_ != kNormal)
    return true;

  // This is called from another thread.
  scoped_refptr<VideoFrame> video_frame;
  {
    base::AutoLock auto_lock(lock_);
    if (frame_queue_available_.size() == 0) {
      return true;
    }
    video_frame = frame_queue_available_.front();
    frame_queue_available_.pop_front();
  }

  // Check if there's a size change.
  // TODO(vrk): Remove casts when media::VideoFrame is updated with gfx::Sizes
  // for width/height.
  if (video_frame->width() != static_cast<size_t>(visible_size_.width()) ||
      video_frame->height() != static_cast<size_t>(visible_size_.height())) {
    // Allocate new buffer based on the new size.
    video_frame = VideoFrame::CreateFrame(VideoFrame::YV12,
                                          visible_size_.width(),
                                          visible_size_.height(),
                                          kNoTimestamp,
                                          kNoTimestamp);
  }

  // Only YV12 frames are supported.
  DCHECK(video_frame->format() == VideoFrame::YV12);
  // Aspect ratio unsupported; DCHECK when there are non-square pixels.
  DCHECK(frame->GetPixelWidth() == 1);
  DCHECK(frame->GetPixelHeight() == 1);
  video_frame->SetTimestamp(host()->GetTime());
  video_frame->SetDuration(base::TimeDelta::FromMilliseconds(30));

  int y_rows = frame->GetHeight();
  int uv_rows = frame->GetHeight() / 2;  // YV12 format.
  CopyYPlane(frame->GetYPlane(), frame->GetYPitch(), y_rows, video_frame);
  CopyUPlane(frame->GetUPlane(), frame->GetUPitch(), uv_rows, video_frame);
  CopyVPlane(frame->GetVPlane(), frame->GetVPitch(), uv_rows, video_frame);

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
