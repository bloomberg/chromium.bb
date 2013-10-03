// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_webrtc_log_message_delegate.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "chrome/common/partial_circular_buffer.h"
#include "chrome/renderer/media/webrtc_logging_message_filter.h"

ChromeWebRtcLogMessageDelegate::ChromeWebRtcLogMessageDelegate(
    const scoped_refptr<base::MessageLoopProxy>& io_message_loop,
    WebRtcLoggingMessageFilter* message_filter)
    : io_message_loop_(io_message_loop),
      message_filter_(message_filter) {
  content::InitWebRtcLoggingDelegate(this);
}

ChromeWebRtcLogMessageDelegate::~ChromeWebRtcLogMessageDelegate() {
  DCHECK(CalledOnValidThread());
}

void ChromeWebRtcLogMessageDelegate::LogMessage(const std::string& message) {
  if (!CalledOnValidThread()) {
    io_message_loop_->PostTask(
        FROM_HERE, base::Bind(
            &ChromeWebRtcLogMessageDelegate::LogMessage,
            base::Unretained(this),
            message));
    return;
  }

  if (circular_buffer_) {
    circular_buffer_->Write(message.c_str(), message.length());
    const char eol = '\n';
    circular_buffer_->Write(&eol, 1);
  }
}

void ChromeWebRtcLogMessageDelegate::OnFilterRemoved() {
  DCHECK(CalledOnValidThread());
  message_filter_ = NULL;
}

void ChromeWebRtcLogMessageDelegate::OnStartLogging(
    base::SharedMemoryHandle handle,
    uint32 length) {
  DCHECK(CalledOnValidThread());
  DCHECK(!shared_memory_ && !circular_buffer_);

  shared_memory_.reset(new base::SharedMemory(handle, false));
  CHECK(shared_memory_->Map(length));
  circular_buffer_.reset(
      new PartialCircularBuffer(shared_memory_->memory(),
                                length,
                                length / 2,
                                true));

  content::InitWebRtcLogging();
}

void ChromeWebRtcLogMessageDelegate::OnStopLogging() {
  DCHECK(CalledOnValidThread());
  DCHECK(shared_memory_ && circular_buffer_);

  circular_buffer_.reset(NULL);
  shared_memory_.reset(NULL);
  if (message_filter_)
    message_filter_->LoggingStopped();
}
