// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/data_pipe_peek.h"

#include <stddef.h>
#include <stdint.h>

#include "mojo/edk/system/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace runner {
namespace {

// In various places, we have to poll (since, e.g., we can't yet wait for a
// certain amount of data to be available). This is the maximum number of
// iterations (separated by a short sleep).
// TODO(vtl): Get rid of this.
const size_t kMaxPoll = 100;

TEST(DataPipePeek, PeekNBytes) {
  DataPipe data_pipe;
  DataPipeConsumerHandle consumer(data_pipe.consumer_handle.get());
  DataPipeProducerHandle producer(data_pipe.producer_handle.get());

  // Inialize the pipe with 4 bytes.

  const char* s4 = "1234";
  uint32_t num_bytes4 = 4;
  EXPECT_EQ(MOJO_RESULT_OK,
            WriteDataRaw(producer, s4, &num_bytes4, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(4u, num_bytes4);

  // We're not consuming data, so peeking for 4 bytes should always succeed.

  std::string bytes;
  MojoDeadline timeout = MOJO_DEADLINE_INDEFINITE;
  EXPECT_TRUE(shell::BlockingPeekNBytes(consumer, &bytes, num_bytes4, timeout));
  EXPECT_EQ(bytes, std::string(s4));

  timeout = 1000;  // 1ms
  EXPECT_TRUE(shell::BlockingPeekNBytes(consumer, &bytes, num_bytes4, timeout));
  EXPECT_EQ(bytes, std::string(s4));

  timeout = MOJO_DEADLINE_INDEFINITE;
  EXPECT_TRUE(shell::BlockingPeekNBytes(consumer, &bytes, num_bytes4, timeout));
  EXPECT_EQ(bytes, std::string(s4));

  // Peeking for 5 bytes should fail, until another byte is written.

  uint32_t bytes1 = 1;
  uint32_t num_bytes5 = 5;
  const char* s1 = "5";
  const char* s5 = "12345";

  timeout = 0;
  EXPECT_FALSE(
      shell::BlockingPeekNBytes(consumer, &bytes, num_bytes5, timeout));

  timeout = 500;  // Should cause peek to timeout after about 0.5ms.
  EXPECT_FALSE(
      shell::BlockingPeekNBytes(consumer, &bytes, num_bytes5, timeout));

  EXPECT_EQ(MOJO_RESULT_OK,
            WriteDataRaw(producer, s1, &bytes1, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(1u, bytes1);

  EXPECT_TRUE(shell::BlockingPeekNBytes(consumer, &bytes, num_bytes5, timeout));
  EXPECT_EQ(bytes, std::string(s5));

  // If the consumer side of the pipe is closed, peek should fail.

  data_pipe.consumer_handle.reset();
  timeout = 0;
  EXPECT_FALSE(
      shell::BlockingPeekNBytes(consumer, &bytes, num_bytes5, timeout));
}

TEST(DataPipePeek, PeekLine) {
  DataPipe data_pipe;
  DataPipeConsumerHandle consumer(data_pipe.consumer_handle.get());
  DataPipeProducerHandle producer(data_pipe.producer_handle.get());

  // Inialize the pipe with 4 bytes and no newline.

  const char* s4 = "1234";
  uint32_t num_bytes4 = 4;
  EXPECT_EQ(MOJO_RESULT_OK,
            WriteDataRaw(producer, s4, &num_bytes4, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(4u, num_bytes4);

  // Peeking for a line should fail.

  std::string str;
  size_t max_str_length = 5;
  MojoDeadline timeout = 0;
  EXPECT_FALSE(
      shell::BlockingPeekLine(consumer, &str, max_str_length, timeout));

  // Writing a newline should cause PeekLine to succeed.

  uint32_t bytes1 = 1;
  const char* s1 = "\n";
  timeout = MOJO_DEADLINE_INDEFINITE;
  EXPECT_EQ(MOJO_RESULT_OK,
            WriteDataRaw(producer, s1, &bytes1, MOJO_WRITE_DATA_FLAG_NONE));
  EXPECT_EQ(1u, bytes1);

  bool succeeded = false;
  for (size_t i = 0; i < kMaxPoll; i++) {
    if (shell::BlockingPeekLine(consumer, &str, max_str_length, timeout)) {
      succeeded = true;
      break;
    }
    edk::test::Sleep(edk::test::EpsilonDeadline());
  }
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(str, std::string(s4) + "\n");

  // If the max_line_length parameter is less than the length of the
  // newline terminated string, then peek should fail.

  max_str_length = 3;
  timeout = 0;
  EXPECT_FALSE(
      shell::BlockingPeekLine(consumer, &str, max_str_length, timeout));
}

}  // namespace
}  // namespace runner
}  // namespace mojo
