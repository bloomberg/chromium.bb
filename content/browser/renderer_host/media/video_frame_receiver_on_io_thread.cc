// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_frame_receiver_on_io_thread.h"

#include "content/public/browser/browser_thread.h"

namespace content {

VideoFrameReceiverOnIOThread::VideoFrameReceiverOnIOThread(
    const base::WeakPtr<VideoFrameReceiver>& receiver)
    : receiver_(receiver) {}

VideoFrameReceiverOnIOThread::~VideoFrameReceiverOnIOThread() = default;

void VideoFrameReceiverOnIOThread::OnIncomingCapturedVideoFrame(
    media::VideoCaptureDevice::Client::Buffer buffer,
    scoped_refptr<media::VideoFrame> frame) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoFrameReceiver::OnIncomingCapturedVideoFrame, receiver_,
                 base::Passed(&buffer), frame));
}

void VideoFrameReceiverOnIOThread::OnError() {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoFrameReceiver::OnError, receiver_));
}

void VideoFrameReceiverOnIOThread::OnLog(const std::string& message) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoFrameReceiver::OnLog, receiver_, message));
}

void VideoFrameReceiverOnIOThread::OnBufferRetired(int buffer_id) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoFrameReceiver::OnBufferRetired, receiver_, buffer_id));
}

}  // namespace content
