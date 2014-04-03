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
      logging_started_(false),
      message_filter_(message_filter) {
  content::InitWebRtcLoggingDelegate(this);
}

ChromeWebRtcLogMessageDelegate::~ChromeWebRtcLogMessageDelegate() {
  DCHECK(CalledOnValidThread());
}

void ChromeWebRtcLogMessageDelegate::LogMessage(const std::string& message) {
  WebRtcLoggingMessageData data(base::Time::Now(), message);

  io_message_loop_->PostTask(
      FROM_HERE, base::Bind(
          &ChromeWebRtcLogMessageDelegate::LogMessageOnIOThread,
          base::Unretained(this),
          data));
}

void ChromeWebRtcLogMessageDelegate::LogMessageOnIOThread(
    const WebRtcLoggingMessageData& message) {
  DCHECK(CalledOnValidThread());

  if (logging_started_ && message_filter_) {
    if (!log_buffer_.empty()) {
      // A delayed task has already been posted for sending the buffer contents.
      // Just add the message to the buffer.
      log_buffer_.push_back(message);
      return;
    }

    log_buffer_.push_back(message);

    if (base::TimeTicks::Now() - last_log_buffer_send_ >
        base::TimeDelta::FromMilliseconds(100)) {
      SendLogBuffer();
    } else {
      io_message_loop_->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ChromeWebRtcLogMessageDelegate::SendLogBuffer,
                     base::Unretained(this)),
          base::TimeDelta::FromMilliseconds(200));
    }
  }
}

void ChromeWebRtcLogMessageDelegate::OnFilterRemoved() {
  DCHECK(CalledOnValidThread());
  message_filter_ = NULL;
}

void ChromeWebRtcLogMessageDelegate::OnStartLogging() {
  DCHECK(CalledOnValidThread());
  logging_started_ = true;
  content::InitWebRtcLogging();
}

void ChromeWebRtcLogMessageDelegate::OnStopLogging() {
  DCHECK(CalledOnValidThread());
  if (!log_buffer_.empty())
    SendLogBuffer();
  if (message_filter_)
    message_filter_->LoggingStopped();
  logging_started_ = false;
}

void ChromeWebRtcLogMessageDelegate::SendLogBuffer() {
  DCHECK(CalledOnValidThread());
  if (logging_started_ && message_filter_) {
    message_filter_->AddLogMessages(log_buffer_);
    last_log_buffer_send_ = base::TimeTicks::Now();
  }
  log_buffer_.clear();
}
