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
      message_filter_(message_filter),
      log_initialized_(false) {
  content::InitWebRtcLoggingDelegate(this);
}

ChromeWebRtcLogMessageDelegate::~ChromeWebRtcLogMessageDelegate() {
  DCHECK(CalledOnValidThread());
}

void ChromeWebRtcLogMessageDelegate::InitLogging(
    const std::string& app_session_id,
    const std::string& app_url) {
  DCHECK(CalledOnValidThread());

  if (!log_initialized_) {
    log_initialized_ = true;
    message_filter_->InitLogging(app_session_id, app_url);
  }
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

void ChromeWebRtcLogMessageDelegate::OnLogOpened(
    base::SharedMemoryHandle handle,
    uint32 length) {
  DCHECK(CalledOnValidThread());

  shared_memory_.reset(new base::SharedMemory(handle, false));
  CHECK(shared_memory_->Map(length));
  circular_buffer_.reset(
      new PartialCircularBuffer(shared_memory_->memory(),
                                length,
                                length / 2,
                                true));
}

void ChromeWebRtcLogMessageDelegate::OnOpenLogFailed() {
  DCHECK(CalledOnValidThread());
  DLOG(ERROR) << "Could not open log.";
  // TODO(grunell): Implement.
  NOTIMPLEMENTED();
}
