// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_subscriber_stdio.h"

#include "base/files/scoped_temp_dir.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TraceSubscriberStdioTest : public ::testing::Test {};

TEST_F(TraceSubscriberStdioTest, CanWriteDataToFile) {
  base::ScopedTempDir trace_dir;
  ASSERT_TRUE(trace_dir.CreateUniqueTempDir());
  base::FilePath trace_file(trace_dir.path().AppendASCII("trace.txt"));
  {
    TraceSubscriberStdio subscriber(trace_file);

    std::string foo("foo");
    subscriber.OnTraceDataCollected(
        make_scoped_refptr(base::RefCountedString::TakeString(&foo)));

    std::string bar("bar");
    subscriber.OnTraceDataCollected(
        make_scoped_refptr(base::RefCountedString::TakeString(&bar)));

    subscriber.OnEndTracingComplete();
  }
  BrowserThread::GetBlockingPool()->FlushForTesting();
  std::string result;
  EXPECT_TRUE(file_util::ReadFileToString(trace_file, &result));
  EXPECT_EQ("[foo,bar]", result);
}

}  // namespace content
