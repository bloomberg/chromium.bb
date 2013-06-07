// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_log_unittest.h"

#include "base/bind.h"
#include "base/values.h"
#include "net/base/net_errors.h"

namespace net {

namespace {

base::Value* NetLogLevelCallback(NetLog::LogLevel log_level) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("log_level", log_level);
  return dict;
}

TEST(NetLogTest, Basic) {
  CapturingNetLog net_log;
  net::CapturingNetLog::CapturedEntryList entries;
  net_log.GetEntries(&entries);
  EXPECT_EQ(0u, entries.size());

  net_log.AddGlobalEntry(NetLog::TYPE_CANCELLED);

  net_log.GetEntries(&entries);
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(NetLog::TYPE_CANCELLED, entries[0].type);
  EXPECT_EQ(NetLog::SOURCE_NONE, entries[0].source.type);
  EXPECT_NE(NetLog::Source::kInvalidId, entries[0].source.id);
  EXPECT_EQ(NetLog::PHASE_NONE, entries[0].phase);
  EXPECT_GE(base::TimeTicks::Now(), entries[0].time);
  EXPECT_FALSE(entries[0].params);
}

// Check that the correct LogLevel is sent to NetLog Value callbacks, and that
// LOG_NONE logs no events.
TEST(NetLogTest, LogLevels) {
  CapturingNetLog net_log;
  for (int log_level = NetLog::LOG_ALL; log_level <= NetLog::LOG_NONE;
       ++log_level) {
    net_log.SetLogLevel(static_cast<NetLog::LogLevel>(log_level));
    EXPECT_EQ(log_level, net_log.GetLogLevel());

    net_log.AddGlobalEntry(NetLog::TYPE_SOCKET_ALIVE,
                           base::Bind(NetLogLevelCallback));

    net::CapturingNetLog::CapturedEntryList entries;
    net_log.GetEntries(&entries);

    if (log_level == NetLog::LOG_NONE) {
      EXPECT_EQ(0u, entries.size());
    } else {
      ASSERT_EQ(1u, entries.size());
      EXPECT_EQ(NetLog::TYPE_SOCKET_ALIVE, entries[0].type);
      EXPECT_EQ(NetLog::SOURCE_NONE, entries[0].source.type);
      EXPECT_NE(NetLog::Source::kInvalidId, entries[0].source.id);
      EXPECT_EQ(NetLog::PHASE_NONE, entries[0].phase);
      EXPECT_GE(base::TimeTicks::Now(), entries[0].time);

      int logged_log_level;
      ASSERT_TRUE(entries[0].GetIntegerValue("log_level", &logged_log_level));
      EXPECT_EQ(log_level, logged_log_level);
    }

    net_log.Clear();
  }
}

}  // namespace

}  // namespace net
