// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/origin_operation_queue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_file_system {

namespace {

void PopAndVerify(const GURL& expected_origin,
                  OriginOperation::Type expected_type,
                  OriginOperationQueue* queue) {
  ASSERT_FALSE(queue->empty());
  OriginOperation op;
  op = queue->Pop();
  EXPECT_EQ(expected_origin, op.origin);
  EXPECT_EQ(expected_type, op.type);
}

}  // namespace

TEST(OriginOperationQueueTest, Simple) {
  GURL origin1("chrome-extension://foo");
  GURL origin2("chrome-extension://bar");
  OriginOperationQueue queue;

  queue.Push(origin1, OriginOperation::REGISTERING);
  queue.Push(origin2, OriginOperation::DISABLING);
  queue.Push(origin1, OriginOperation::UNINSTALLING);

  ASSERT_EQ(3U, queue.size());
  ASSERT_TRUE(queue.HasPendingOperation(origin1));
  ASSERT_TRUE(queue.HasPendingOperation(origin2));

  PopAndVerify(origin1, OriginOperation::REGISTERING, &queue);
  PopAndVerify(origin2, OriginOperation::DISABLING, &queue);

  ASSERT_TRUE(queue.HasPendingOperation(origin1));
  ASSERT_FALSE(queue.HasPendingOperation(origin2));

  PopAndVerify(origin1, OriginOperation::UNINSTALLING, &queue);

  ASSERT_TRUE(queue.empty());
  ASSERT_FALSE(queue.HasPendingOperation(origin1));
  ASSERT_FALSE(queue.HasPendingOperation(origin2));
}

}  // namespace sync_file_system
