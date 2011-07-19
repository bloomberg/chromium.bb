// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/capturing_net_log.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(NetLog, ScopedNetLogEventTest) {
  CapturingNetLog log(CapturingNetLog::kUnbounded);
  BoundNetLog net_log(BoundNetLog::Make(&log, NetLog::SOURCE_URL_REQUEST));

  scoped_ptr<ScopedNetLogEvent> net_log_event(
      new ScopedNetLogEvent(net_log, NetLog::TYPE_REQUEST_ALIVE, NULL));

  CapturingNetLog::EntryList entries;
  log.GetEntries(&entries);
  EXPECT_EQ(1u, entries.size());
  EXPECT_TRUE(LogContainsBeginEvent(entries, 0, NetLog::TYPE_REQUEST_ALIVE));

  net_log_event.reset();
  log.GetEntries(&entries);
  EXPECT_EQ(2u, entries.size());
  EXPECT_TRUE(LogContainsEndEvent(entries, 1, NetLog::TYPE_REQUEST_ALIVE));
}

}  // namespace

}  // namespace net
