// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/test_completion_callback.h"
#include "net/socket/socket_test_util.h"
#include "testing/platform_test.h"

#include "net/flip/flip_session.h"

namespace net {

class FlipSessionTest : public PlatformTest {
 public:
};

// Test the PrioritizedIOBuffer class.
TEST_F(FlipSessionTest, PrioritizedIOBuffer) {
  std::priority_queue<PrioritizedIOBuffer> queue_;
  const int kQueueSize = 100;

  // Insert 100 items; pri 100 to 1.
  for (int index = 0; index < kQueueSize; ++index) {
    PrioritizedIOBuffer buffer(NULL, kQueueSize - index);
    queue_.push(buffer);
  }

  // Insert several priority 0 items last.
  const int kNumDuplicates = 12;
  IOBufferWithSize* buffers[kNumDuplicates];
  for (int index = 0; index < kNumDuplicates; ++index) {
    buffers[index] = new IOBufferWithSize(index+1);
    queue_.push(PrioritizedIOBuffer(buffers[index], 0));
  }

  EXPECT_EQ(kQueueSize + kNumDuplicates, queue_.size());

  // Verify the P0 items come out in FIFO order.
  for (int index = 0; index < kNumDuplicates; ++index) {
    PrioritizedIOBuffer buffer = queue_.top();
    EXPECT_EQ(0, buffer.priority());
    EXPECT_EQ(index + 1, buffer.size());
    queue_.pop();
  }

  int priority = 1;
  while (queue_.size()) {
    PrioritizedIOBuffer buffer = queue_.top();
    EXPECT_EQ(priority++, buffer.priority());
    queue_.pop();
  }
}

}  // namespace net

