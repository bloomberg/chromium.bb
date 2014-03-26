// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/rappor/log_uploader.h"

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop_proxy.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace rappor {

namespace {

const char kTestServerURL[] = "http://a.com/";
const char kTestMimeType[] = "text/plain";

class TestLogUploader : public LogUploader {
 public:
  TestLogUploader(net::URLRequestContextGetter* request_context) :
      LogUploader(GURL(kTestServerURL), kTestMimeType, request_context) {
  }

  base::TimeDelta last_interval_set() const { return last_interval_set_; };

  void StartUpload() {
    last_interval_set_ = base::TimeDelta();
    StartScheduledUpload();
  }

  static base::TimeDelta BackOff(base::TimeDelta t) {
    return LogUploader::BackOffUploadInterval(t);
  }

 protected:
  virtual bool IsUploadScheduled() const OVERRIDE {
    return last_interval_set() != base::TimeDelta();
  }

  // Schedules a future call to StartScheduledUpload if one isn't already
  // pending.
  virtual void ScheduleNextUpload(base::TimeDelta interval) OVERRIDE {
    EXPECT_EQ(last_interval_set(), base::TimeDelta());
    last_interval_set_ = interval;
  }

  base::TimeDelta last_interval_set_;

  DISALLOW_COPY_AND_ASSIGN(TestLogUploader);
};

}  // namespace

class LogUploaderTest : public testing::Test {
 public:
  LogUploaderTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::MessageLoopProxy::current())),
        factory_(NULL) {}

 protected:
  // Required for base::MessageLoopProxy::current().
  base::MessageLoopForUI loop_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::FakeURLFetcherFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(LogUploaderTest);
};

TEST_F(LogUploaderTest, Success) {
  TestLogUploader uploader(request_context_);

  factory_.SetFakeResponse(GURL(kTestServerURL),
                           std::string(),
                           net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  uploader.QueueLog("log1");
  base::MessageLoop::current()->RunUntilIdle();
  // Log should be discarded instead of retransmitted.
  EXPECT_EQ(uploader.last_interval_set(), base::TimeDelta());
}

TEST_F(LogUploaderTest, Rejection) {
  TestLogUploader uploader(request_context_);

  factory_.SetFakeResponse(GURL(kTestServerURL),
                           std::string(),
                           net::HTTP_BAD_REQUEST,
                           net::URLRequestStatus::SUCCESS);

  uploader.QueueLog("log1");
  base::MessageLoop::current()->RunUntilIdle();
  // Log should be discarded instead of retransmitted.
  EXPECT_EQ(uploader.last_interval_set(), base::TimeDelta());
}

TEST_F(LogUploaderTest, Failure) {
  TestLogUploader uploader(request_context_);

  factory_.SetFakeResponse(GURL(kTestServerURL),
                           std::string(),
                           net::HTTP_INTERNAL_SERVER_ERROR,
                           net::URLRequestStatus::SUCCESS);

  uploader.QueueLog("log1");
  base::MessageLoop::current()->RunUntilIdle();
  // Log should be scheduled for retransmission.
  base::TimeDelta error_interval = uploader.last_interval_set();
  EXPECT_GT(error_interval, base::TimeDelta());

  for (int i = 0; i < 10; i++) {
    uploader.QueueLog("logX");
  }

  // A second failure should lead to a longer interval, and the log should
  // be discarded due to full queue.
  uploader.StartUpload();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_GT(uploader.last_interval_set(), error_interval);

  factory_.SetFakeResponse(GURL(kTestServerURL),
                           std::string(),
                           net::HTTP_OK,
                           net::URLRequestStatus::SUCCESS);

  // A success should revert to base interval while queue is not empty.
  for (int i = 0; i < 9; i++) {
    uploader.StartUpload();
    base::MessageLoop::current()->RunUntilIdle();
    EXPECT_LT(uploader.last_interval_set(), error_interval);
  }

  // Queue should be empty.
  uploader.StartUpload();
  base::MessageLoop::current()->RunUntilIdle();
  EXPECT_EQ(uploader.last_interval_set(), base::TimeDelta());
}

TEST_F(LogUploaderTest, Backoff) {
  base::TimeDelta current = base::TimeDelta();
  base::TimeDelta next = base::TimeDelta::FromSeconds(1);
  // Backoff until the maximum is reached.
  while (next > current) {
    current = next;
    next = TestLogUploader::BackOff(current);
  }
  // Maximum backoff should have been reached.
  EXPECT_EQ(next, current);
}

}  // namespace rappor
