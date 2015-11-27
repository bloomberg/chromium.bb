// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/media/webrtc_logging_message_data.h"

#include "base/strings/stringprintf.h"

WebRtcLoggingMessageData::WebRtcLoggingMessageData() {}

WebRtcLoggingMessageData::WebRtcLoggingMessageData(base::Time time,
                                                   const std::string& message)
    : timestamp(time), message(message) {}

std::string
WebRtcLoggingMessageData::Format(base::Time start_time) const {
  int32 interval_ms =
      static_cast<int32>((timestamp - start_time).InMilliseconds());

  std::string result = base::StringPrintf("[%03d:%03d] %s",
                                          interval_ms / 1000,
                                          interval_ms % 1000,
                                          message.c_str());
  return result;
}
