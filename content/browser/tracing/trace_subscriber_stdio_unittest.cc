// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/trace_subscriber_stdio.h"

#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class TraceSubscriberStdioTest : public ::testing::Test {};

TEST_F(TraceSubscriberStdioTest, CanWriteArray) {
  base::ScopedTempDir trace_dir;
  ASSERT_TRUE(trace_dir.CreateUniqueTempDir());
  base::FilePath trace_file(trace_dir.path().AppendASCII("trace.txt"));
  {
    TraceSubscriberStdio subscriber(trace_file,
                                    TraceSubscriberStdio::FILE_TYPE_ARRAY,
                                    false);

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
  EXPECT_TRUE(base::ReadFileToString(trace_file, &result));
  EXPECT_EQ("[foo,bar]", result);
}

TEST_F(TraceSubscriberStdioTest, CanWritePropertyList) {
  base::ScopedTempDir trace_dir;
  ASSERT_TRUE(trace_dir.CreateUniqueTempDir());
  base::FilePath trace_file(trace_dir.path().AppendASCII("trace.txt"));
  {
    TraceSubscriberStdio subscriber(
        trace_file,
        TraceSubscriberStdio::FILE_TYPE_PROPERTY_LIST,
        false);

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
  EXPECT_TRUE(base::ReadFileToString(trace_file, &result));
  EXPECT_EQ("{\"traceEvents\":[foo,bar]}", result);
}

TEST_F(TraceSubscriberStdioTest, CanWriteSystemDataFirst) {
  base::ScopedTempDir trace_dir;
  ASSERT_TRUE(trace_dir.CreateUniqueTempDir());
  base::FilePath trace_file(trace_dir.path().AppendASCII("trace.txt"));
  {
    TraceSubscriberStdio subscriber(
        trace_file,
        TraceSubscriberStdio::FILE_TYPE_PROPERTY_LIST,
        true);

    std::string foo("foo");
    subscriber.OnTraceDataCollected(
        make_scoped_refptr(base::RefCountedString::TakeString(&foo)));

    std::string bar("bar");
    subscriber.OnTraceDataCollected(
        make_scoped_refptr(base::RefCountedString::TakeString(&bar)));

    std::string systemTrace("event1\nev\"ent\"2\n");
    subscriber.OnEndSystemTracing(
        make_scoped_refptr(base::RefCountedString::TakeString(&systemTrace)));
    subscriber.OnEndTracingComplete();
  }
  BrowserThread::GetBlockingPool()->FlushForTesting();
  std::string result;
  EXPECT_TRUE(base::ReadFileToString(trace_file, &result));
  EXPECT_EQ(
    "{\"traceEvents\":[foo,bar],\""
    "systemTraceEvents\":\"event1\\nev\\\"ent\\\"2\\n\"}",
    result);
}

TEST_F(TraceSubscriberStdioTest, CanWriteSystemDataLast) {
  base::ScopedTempDir trace_dir;
  ASSERT_TRUE(trace_dir.CreateUniqueTempDir());
  base::FilePath trace_file(trace_dir.path().AppendASCII("trace.txt"));
  {
    TraceSubscriberStdio subscriber(
        trace_file,
        TraceSubscriberStdio::FILE_TYPE_PROPERTY_LIST,
        true);

    std::string foo("foo");
    subscriber.OnTraceDataCollected(
        make_scoped_refptr(base::RefCountedString::TakeString(&foo)));

    std::string bar("bar");
    subscriber.OnTraceDataCollected(
        make_scoped_refptr(base::RefCountedString::TakeString(&bar)));

    std::string systemTrace("event1\nev\"ent\"2\n");
    subscriber.OnEndTracingComplete();
    subscriber.OnEndSystemTracing(
        make_scoped_refptr(base::RefCountedString::TakeString(&systemTrace)));
  }
  BrowserThread::GetBlockingPool()->FlushForTesting();
  std::string result;
  EXPECT_TRUE(base::ReadFileToString(trace_file, &result));
  EXPECT_EQ(
    "{\"traceEvents\":[foo,bar],\""
    "systemTraceEvents\":\"event1\\nev\\\"ent\\\"2\\n\"}",
    result);
}

}  // namespace content
