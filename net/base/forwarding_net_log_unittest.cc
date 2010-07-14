// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/forwarding_net_log.h"

#include "base/message_loop.h"
#include "net/base/capturing_net_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Test forwarding a call to AddEntry() to another implementation, operating
// on this same message loop.
TEST(ForwardingNetLogTest, Basic) {
  // Create a forwarding NetLog that sends messages to this same thread.
  CapturingNetLog log(CapturingNetLog::kUnbounded);
  ForwardingNetLog forwarding(&log, MessageLoop::current());

  EXPECT_EQ(0u, log.entries().size());

  NetLogStringParameter* params = new NetLogStringParameter("xxx", "yyy");

  forwarding.AddEntry(
      NetLog::TYPE_PAC_JAVASCRIPT_ALERT,
      base::TimeTicks(),
      NetLog::Source(),
      NetLog::PHASE_NONE,
      params);

  // Should still be empty, since we posted an async task.
  EXPECT_EQ(0u, log.entries().size());

  MessageLoop::current()->RunAllPending();

  // After draining the message loop, we should now have executed the task
  // and hence emitted the log entry.
  ASSERT_EQ(1u, log.entries().size());

  // Check that the forwarded call contained received all the right inputs.
  EXPECT_EQ(NetLog::TYPE_PAC_JAVASCRIPT_ALERT, log.entries()[0].type);
  EXPECT_EQ(NetLog::SOURCE_NONE, log.entries()[0].source.type);
  EXPECT_EQ(NetLog::PHASE_NONE, log.entries()[0].phase);
  EXPECT_EQ(params, log.entries()[0].extra_parameters.get());

  // Check that the parameters is still referenced. (if the reference was
  // lost then this will be a memory error and probaby crash).
  EXPECT_EQ("yyy", params->value());
}

// Test forwarding a call to AddEntry() to another implementation that runs
// on the same message loop. However destroy the forwarder before the posted
// task has a chance to run.
TEST(ForwardingNetLogTest, Orphan) {
  // Create a forwarding NetLog that sends messages to this same thread.
  CapturingNetLog log(CapturingNetLog::kUnbounded);
  {
    ForwardingNetLog forwarding(&log, MessageLoop::current());
    EXPECT_EQ(0u, log.entries().size());

    forwarding.AddEntry(
        NetLog::TYPE_PAC_JAVASCRIPT_ALERT,
        base::TimeTicks(),
        NetLog::Source(),
        NetLog::PHASE_NONE,
        NULL);

    // Should still be empty, since we posted an async task.
    EXPECT_EQ(0u, log.entries().size());
  }

  // At this point the ForwardingNetLog is deleted. However it had already
  // posted a task to the message loop. Once we drain the message loop, we
  // verify that the task didn't actually try to emit to the NetLog.
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(0u, log.entries().size());
}

}  // namespace

}  // namespace net

