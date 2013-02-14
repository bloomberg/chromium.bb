// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/net_error_tracker.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef NetErrorTracker::FrameType FrameType;
typedef NetErrorTracker::PageType PageType;
typedef NetErrorTracker::ErrorType ErrorType;

const FrameType FRAME_SUB = NetErrorTracker::FRAME_SUB;
const FrameType FRAME_MAIN = NetErrorTracker::FRAME_MAIN;

const PageType PAGE_NORMAL = NetErrorTracker::PAGE_NORMAL;
const PageType PAGE_ERROR = NetErrorTracker::PAGE_ERROR;

const ErrorType ERROR_OTHER = NetErrorTracker::ERROR_OTHER;
const ErrorType ERROR_DNS = NetErrorTracker::ERROR_DNS;

class NetErrorTrackerTest : public testing::Test {
 public:
  NetErrorTrackerTest()
      : ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(
            base::Bind(&NetErrorTrackerTest::TrackerCallback,
                       base::Unretained(this)))),
        callback_state_(NetErrorTracker::DNS_ERROR_PAGE_NONE),
        callback_count_(0) {
  }

 protected:
  NetErrorTracker tracker_;
  NetErrorTracker::DnsErrorPageState callback_state_;
  int callback_count_;

 private:
  void TrackerCallback(NetErrorTracker::DnsErrorPageState state) {
    callback_state_ = state;
    ++callback_count_;
  }
};

TEST_F(NetErrorTrackerTest, InitialState) {
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(NetErrorTracker::DNS_ERROR_PAGE_NONE, callback_state_);
}

TEST_F(NetErrorTrackerTest, SuccessfulMainFrameLoad) {
  tracker_.OnStartProvisionalLoad(FRAME_MAIN, PAGE_NORMAL);
  tracker_.OnCommitProvisionalLoad(FRAME_MAIN);
  tracker_.OnFinishLoad(FRAME_MAIN);

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(NetErrorTracker::DNS_ERROR_PAGE_NONE, callback_state_);
}

TEST_F(NetErrorTrackerTest, SuccessfulSubFrameLoad) {
  tracker_.OnStartProvisionalLoad(FRAME_SUB, PAGE_NORMAL);
  tracker_.OnCommitProvisionalLoad(FRAME_SUB);
  tracker_.OnFinishLoad(FRAME_SUB);

  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(NetErrorTracker::DNS_ERROR_PAGE_NONE, callback_state_);
}

TEST_F(NetErrorTrackerTest, FailedMainFrameLoad) {
  tracker_.OnStartProvisionalLoad(FRAME_MAIN, PAGE_NORMAL);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(NetErrorTracker::DNS_ERROR_PAGE_NONE, callback_state_);

  tracker_.OnFailProvisionalLoad(FRAME_MAIN, ERROR_DNS);
  EXPECT_EQ(1, callback_count_);
  EXPECT_EQ(NetErrorTracker::DNS_ERROR_PAGE_PENDING, callback_state_);

  tracker_.OnStartProvisionalLoad(FRAME_MAIN, PAGE_ERROR);
  tracker_.OnCommitProvisionalLoad(FRAME_MAIN);
  tracker_.OnFinishLoad(FRAME_MAIN);
  EXPECT_EQ(2, callback_count_);
  EXPECT_EQ(NetErrorTracker::DNS_ERROR_PAGE_LOADED, callback_state_);

  tracker_.OnStartProvisionalLoad(FRAME_MAIN, PAGE_NORMAL);
  EXPECT_EQ(3, callback_count_);
  EXPECT_EQ(NetErrorTracker::DNS_ERROR_PAGE_NONE, callback_state_);
}

TEST_F(NetErrorTrackerTest, FailedSubFrameLoad) {
  tracker_.OnStartProvisionalLoad(FRAME_SUB, PAGE_NORMAL);
  tracker_.OnFailProvisionalLoad(FRAME_SUB, ERROR_DNS);
  tracker_.OnStartProvisionalLoad(FRAME_SUB, PAGE_ERROR);
  tracker_.OnCommitProvisionalLoad(FRAME_SUB);
  tracker_.OnFinishLoad(FRAME_SUB);
  EXPECT_EQ(0, callback_count_);
  EXPECT_EQ(NetErrorTracker::DNS_ERROR_PAGE_NONE, callback_state_);
}

}  // namespace
