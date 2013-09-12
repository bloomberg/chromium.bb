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
                  bool expected_aborted,
                  OriginOperationQueue* queue) {
  ASSERT_FALSE(queue->empty());
  OriginOperation op;
  op = queue->Pop();
  EXPECT_EQ(expected_origin, op.origin);
  EXPECT_EQ(expected_type, op.type);
  EXPECT_EQ(expected_aborted, op.aborted);
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

  PopAndVerify(origin1, OriginOperation::REGISTERING, false, &queue);
  PopAndVerify(origin2, OriginOperation::DISABLING, false, &queue);

  ASSERT_TRUE(queue.HasPendingOperation(origin1));
  ASSERT_FALSE(queue.HasPendingOperation(origin2));

  PopAndVerify(origin1, OriginOperation::UNINSTALLING, false, &queue);

  ASSERT_TRUE(queue.empty());
  ASSERT_FALSE(queue.HasPendingOperation(origin1));
  ASSERT_FALSE(queue.HasPendingOperation(origin2));
}

TEST(OriginOperationQueueTest, Abort) {
  GURL origin("chrome-extension://foo");

  OriginOperationQueue queue;
  OriginOperation op;

  // Reload case.
  queue.Push(origin, OriginOperation::REGISTERING);
  queue.Push(origin, OriginOperation::DISABLING);
  queue.Push(origin, OriginOperation::ENABLING);

  ASSERT_EQ(3U, queue.size());
  ASSERT_TRUE(queue.HasPendingOperation(origin));

  PopAndVerify(origin, OriginOperation::REGISTERING, false, &queue);

  // We have two pending operations, DISABLING and ENABLING,
  // and both must have been aborted.
  ASSERT_EQ(2U, queue.size());
  ASSERT_FALSE(queue.HasPendingOperation(origin));

  // Add another registering (like typical reload).
  queue.Push(origin, OriginOperation::REGISTERING);
  ASSERT_EQ(3U, queue.size());
  ASSERT_TRUE(queue.HasPendingOperation(origin));

  PopAndVerify(origin, OriginOperation::DISABLING, true, &queue);
  PopAndVerify(origin, OriginOperation::ENABLING, true, &queue);
  PopAndVerify(origin, OriginOperation::REGISTERING, false, &queue);

  ASSERT_TRUE(queue.empty());
}

TEST(OriginOperationQueueTest, AbortMultipleOrigin) {
  GURL origin1("chrome-extension://foo");
  GURL origin2("chrome-extension://bar");

  OriginOperationQueue queue;
  OriginOperation op;

  queue.Push(origin1, OriginOperation::ENABLING);
  queue.Push(origin2, OriginOperation::DISABLING);  // [1]

  ASSERT_EQ(2U, queue.size());

  // ENABLE and DISABLE on different origins.  They should not be aborted.
  ASSERT_TRUE(queue.HasPendingOperation(origin1));
  ASSERT_TRUE(queue.HasPendingOperation(origin2));

  PopAndVerify(origin1, OriginOperation::ENABLING, false, &queue);

  // Add another ENABLING for origin2.
  queue.Push(origin2, OriginOperation::ENABLING);  // [2]
  ASSERT_EQ(2U, queue.size());

  // Now operation [1] and [2] on origin2 are balanced and aborted.
  ASSERT_FALSE(queue.HasPendingOperation(origin2));

  PopAndVerify(origin2, OriginOperation::DISABLING, true, &queue);
  PopAndVerify(origin2, OriginOperation::ENABLING, true, &queue);

  ASSERT_TRUE(queue.empty());
}

}  // namespace sync_file_system
