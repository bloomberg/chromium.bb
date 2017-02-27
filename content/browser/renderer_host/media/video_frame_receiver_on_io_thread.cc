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

void VideoFrameReceiverOnIOThread::OnNewBufferHandle(
    int buffer_id,
    std::unique_ptr<media::VideoCaptureDevice::Client::Buffer::HandleProvider>
        handle_provider) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoFrameReceiver::OnNewBufferHandle, receiver_, buffer_id,
                 base::Passed(std::move(handle_provider))));
}

void VideoFrameReceiverOnIOThread::OnFrameReadyInBuffer(
    int buffer_id,
    int frame_feedback_id,
    std::unique_ptr<
        media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
        buffer_read_permission,
    media::mojom::VideoFrameInfoPtr frame_info) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoFrameReceiver::OnFrameReadyInBuffer, receiver_,
                 buffer_id, frame_feedback_id,
                 base::Passed(&buffer_read_permission),
                 base::Passed(&frame_info)));
}

void VideoFrameReceiverOnIOThread::OnBufferRetired(int buffer_id) {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoFrameReceiver::OnBufferRetired, receiver_, buffer_id));
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

void VideoFrameReceiverOnIOThread::OnStarted() {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&VideoFrameReceiver::OnStarted, receiver_));
}

}  // namespace content
