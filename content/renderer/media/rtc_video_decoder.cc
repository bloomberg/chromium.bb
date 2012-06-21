// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/rtc_video_decoder.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "media/base/demuxer.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "third_party/libjingle/source/talk/session/phone/videoframe.h"

using media::CopyUPlane;
using media::CopyVPlane;
using media::CopyYPlane;
using media::DemuxerStream;
using media::kNoTimestamp;
using media::PIPELINE_OK;
using media::PipelineStatusCB;
using media::StatisticsCB;
using media::VideoDecoder;

RTCVideoDecoder::RTCVideoDecoder(MessageLoop* message_loop,
                                 const std::string& url)
    : message_loop_(message_loop),
      visible_size_(640, 480),
      url_(url),
      state_(kUnInitialized),
      got_first_frame_(false),
      shutting_down_(false) {
}

void RTCVideoDecoder::Initialize(const scoped_refptr<DemuxerStream>& stream,
                                 const PipelineStatusCB& status_cb,
                                 const StatisticsCB& statistics_cb) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RTCVideoDecoder::Initialize, this,
                   stream, status_cb, statistics_cb));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);
  state_ = kNormal;
  status_cb.Run(PIPELINE_OK);

  // TODO(acolwell): Implement stats.
}

void RTCVideoDecoder::Read(const ReadCB& read_cb) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&RTCVideoDecoder::Read, this, read_cb));
    return;
  }
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  base::AutoLock auto_lock(lock_);
  CHECK(read_cb_.is_null());
  read_cb_ = read_cb;

  if (shutting_down_) {
    message_loop_->PostTask(FROM_HERE,
        base::Bind(&RTCVideoDecoder::CancelPendingRead, this));
  }
}

void RTCVideoDecoder::Reset(const base::Closure& closure) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&RTCVideoDecoder::Reset, this, closure));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  CancelPendingRead();
  closure.Run();
}

void RTCVideoDecoder::Stop(const base::Closure& closure) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&RTCVideoDecoder::Stop, this, closure));
    return;
  }

  DCHECK_EQ(MessageLoop::current(), message_loop_);

  state_ = kStopped;

  closure.Run();
}

const gfx::Size& RTCVideoDecoder::natural_size() {
  // TODO(vrk): Return natural size when aspect ratio support is implemented.
  return visible_size_;
}

void RTCVideoDecoder::PrepareForShutdownHack() {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE,
                            base::Bind(&RTCVideoDecoder::PrepareForShutdownHack,
                                       this));
    return;
  }
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  shutting_down_ = true;
  CancelPendingRead();
}

bool RTCVideoDecoder::SetSize(int width, int height, int reserved) {
  visible_size_.SetSize(width, height);
  return true;
}

// TODO(wjia): this function can be split into two parts so that the |lock_|
// can be removed.
// First creates media::VideoFrame, then post a task onto |message_loop_|
// to deliver that frame.
bool RTCVideoDecoder::RenderFrame(const cricket::VideoFrame* frame) {
  // Called from libjingle thread.
  DCHECK(frame);

  base::TimeDelta timestamp = base::TimeDelta::FromMilliseconds(
      frame->GetTimeStamp() / talk_base::kNumNanosecsPerMillisec);

  ReadCB read_cb;
  {
    base::AutoLock auto_lock(lock_);
    if (read_cb_.is_null() || state_ != kNormal) {
      // TODO(ronghuawu): revisit TS adjustment when crbug.com/111672 is
      // resolved.
      if (got_first_frame_) {
        start_time_ += timestamp - last_frame_timestamp_;
      }
      last_frame_timestamp_ = timestamp;
      return true;
    }
    std::swap(read_cb, read_cb_);
  }

  // Rebase timestamp with zero as starting point.
  if (!got_first_frame_) {
    start_time_ = timestamp;
    got_first_frame_ = true;
  }

  // Always allocate a new frame.
  //
  // TODO(scherkus): migrate this to proper buffer recycling.
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateFrame(media::VideoFrame::YV12,
                                     visible_size_.width(),
                                     visible_size_.height(),
                                     timestamp - start_time_,
                                     base::TimeDelta::FromMilliseconds(0));
  last_frame_timestamp_ = timestamp;

  // Aspect ratio unsupported; DCHECK when there are non-square pixels.
  DCHECK_EQ(frame->GetPixelWidth(), 1u);
  DCHECK_EQ(frame->GetPixelHeight(), 1u);

  int y_rows = frame->GetHeight();
  int uv_rows = frame->GetHeight() / 2;  // YV12 format.
  CopyYPlane(frame->GetYPlane(), frame->GetYPitch(), y_rows, video_frame);
  CopyUPlane(frame->GetUPlane(), frame->GetUPitch(), uv_rows, video_frame);
  CopyVPlane(frame->GetVPlane(), frame->GetVPitch(), uv_rows, video_frame);

  read_cb.Run(kOk, video_frame);
  return true;
}

void RTCVideoDecoder::CancelPendingRead() {
  DCHECK_EQ(MessageLoop::current(), message_loop_);
  ReadCB read_cb;
  {
    base::AutoLock auto_lock(lock_);
    std::swap(read_cb, read_cb_);
  }

  if (!read_cb.is_null()) {
    read_cb.Run(kOk, NULL);
  }
}

RTCVideoDecoder::~RTCVideoDecoder() {}
