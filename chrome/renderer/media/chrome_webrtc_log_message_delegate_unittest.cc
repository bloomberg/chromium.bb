// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/process/process_handle.h"
#include "chrome/common/partial_circular_buffer.h"
#include "chrome/renderer/media/chrome_webrtc_log_message_delegate.h"
#include "chrome/renderer/media/mock_webrtc_logging_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeWebRtcLogMessageDelegateTest, Basic) {
  const char kTestString[] = "abcdefghijklmnopqrstuvwxyz";

  base::MessageLoop message_loop(base::MessageLoop::TYPE_IO);

  scoped_refptr<MockWebRtcLoggingMessageFilter> log_message_filter(
      new MockWebRtcLoggingMessageFilter(message_loop.message_loop_proxy()));

  scoped_ptr<ChromeWebRtcLogMessageDelegate> log_message_delegate(
      new ChromeWebRtcLogMessageDelegate(message_loop.message_loop_proxy(),
                                         log_message_filter));

  log_message_delegate->OnStartLogging();

  // These log messages should be added to the log buffer.
  log_message_delegate->LogMessage(kTestString);
  log_message_delegate->LogMessage(kTestString);

  log_message_delegate->OnStopLogging();

  // This log message should not be added to the log buffer.
  log_message_delegate->LogMessage(kTestString);

  // Size is calculated as (sizeof(kTestString) - 1 for terminating null
  // + 1 for eol added for each log message in LogMessage) * 2.
  const uint32 kExpectedSize = sizeof(kTestString) * 2;
  EXPECT_EQ(kExpectedSize, log_message_filter->log_buffer_.size());

  std::string ref_output = kTestString;
  ref_output.append("\n");
  ref_output.append(kTestString);
  ref_output.append("\n");
  EXPECT_STREQ(ref_output.c_str(), log_message_filter->log_buffer_.c_str());
}
