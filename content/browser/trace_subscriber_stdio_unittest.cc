// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/trace_subscriber_stdio.h"

#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TraceSubscriberStdioTest : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(trace_dir_.CreateUniqueTempDir());
    trace_file_ = trace_dir_.path().AppendASCII("trace.txt");
  }

  std::string ReadTraceFile() {
    std::string result;
    EXPECT_TRUE(file_util::ReadFileToString(trace_file_, &result));
    return result;
  }

  ScopedTempDir trace_dir_;
  FilePath trace_file_;
};

}  // namespace

TEST_F(TraceSubscriberStdioTest, CanWriteBracketedDataToFile) {
  TraceSubscriberStdio subscriber(trace_file_);
  subscriber.OnTraceDataCollected("[foo]");
  subscriber.OnTraceDataCollected("[bar]");
  EXPECT_TRUE(subscriber.IsValid());

  subscriber.OnEndTracingComplete();
  EXPECT_FALSE(subscriber.IsValid());

  EXPECT_EQ("[foo,bar,]", ReadTraceFile());
}

