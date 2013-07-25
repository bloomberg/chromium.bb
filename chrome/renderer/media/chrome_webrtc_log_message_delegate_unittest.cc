// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/process/process_handle.h"
#include "chrome/common/partial_circular_buffer.h"
#include "chrome/renderer/media/chrome_webrtc_log_message_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeWebRtcLogMessageDelegateTest, Basic) {
  const uint32 kTestLogSize = 1024;  // 1 KB
  const char kTestString[] = "abcdefghijklmnopqrstuvwxyz";

  base::MessageLoop message_loop(base::MessageLoop::TYPE_IO);

  scoped_ptr<ChromeWebRtcLogMessageDelegate> log_message_delegate(
      new ChromeWebRtcLogMessageDelegate(message_loop.message_loop_proxy(),
                                         NULL));

  base::SharedMemory shared_memory;
  ASSERT_TRUE(shared_memory.CreateAndMapAnonymous(kTestLogSize));
  base::SharedMemoryHandle new_handle;
  ASSERT_TRUE(shared_memory.ShareToProcess(base::GetCurrentProcessHandle(),
                                           &new_handle));
  log_message_delegate->OnLogOpened(new_handle, kTestLogSize);

  log_message_delegate->LogMessage(kTestString);
  log_message_delegate->LogMessage(kTestString);

  PartialCircularBuffer read_pcb(
      reinterpret_cast<uint8*>(shared_memory.memory()), kTestLogSize);

  // Size is calculated as (sizeof(kTestString) - 1 for terminating null
  // + 1 for eol added for each log message in LogMessage) * 2 + 1 for
  // terminating null.
  char read_buffer[sizeof(kTestString) * 2 + 1] = {0};

  uint32 read = read_pcb.Read(read_buffer, sizeof(read_buffer));
  EXPECT_EQ(sizeof(read_buffer) - 1, read);
  std::string ref_output = kTestString;
  ref_output.append("\n");
  ref_output.append(kTestString);
  ref_output.append("\n");
  EXPECT_STREQ(ref_output.c_str(), read_buffer);
}
