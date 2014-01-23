// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/renderer/media/chrome_webrtc_log_message_delegate.h"
#include "chrome/renderer/media/mock_webrtc_logging_message_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeWebRtcLogMessageDelegateTest, Basic) {
  const char kTestString[] = "abcdefghijklmnopqrstuvwxyz";
  base::MessageLoopForIO message_loop;
  scoped_refptr<MockWebRtcLoggingMessageFilter> log_message_filter(
      new MockWebRtcLoggingMessageFilter(message_loop.message_loop_proxy()));
  // Run message loop to initialize delegate.
  // TODO(vrk): Fix this so that we can construct a delegate without needing to
  // construct a message filter.
  message_loop.RunUntilIdle();

  ChromeWebRtcLogMessageDelegate* log_message_delegate =
      log_message_filter->log_message_delegate();

  // Start logging on the IO loop.
  message_loop.message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(
          &ChromeWebRtcLogMessageDelegate::OnStartLogging,
          base::Unretained(log_message_delegate)));

  // These log messages should be added to the log buffer outside of the IO
  // loop.
  log_message_delegate->LogMessage(kTestString);
  log_message_delegate->LogMessage(kTestString);

  // Stop logging on IO loop.
  message_loop.message_loop_proxy()->PostTask(
      FROM_HERE, base::Bind(
          &ChromeWebRtcLogMessageDelegate::OnStopLogging,
          base::Unretained(log_message_delegate)));

  // This log message should not be added to the log buffer.
  log_message_delegate->LogMessage(kTestString);

  message_loop.RunUntilIdle();

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
