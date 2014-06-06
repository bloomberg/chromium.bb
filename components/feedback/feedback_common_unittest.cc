// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feedback/feedback_common.h"

#include "base/bind.h"
#include "components/feedback/proto/common.pb.h"
#include "components/feedback/proto/dom.pb.h"
#include "components/feedback/proto/extension.pb.h"
#include "components/feedback/proto/math.pb.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kOne[] = "one";
const char kTwo[] = "two";
const char kThree[] = "three";
const char kFour[] = "four";
#define TEN_LINES "0\n1\n2\n3\n4\n5\n6\n7\n8\n9\n"
const char kLongLog[] = TEN_LINES TEN_LINES TEN_LINES TEN_LINES TEN_LINES;
const char kLogsAttachmentName[] = "system_logs.zip";
}  // namespace

class FeedbackCommonTest : public testing::Test {
 protected:
  FeedbackCommonTest() {
    feedback = scoped_refptr<FeedbackCommon>(new FeedbackCommon());
  }

  virtual ~FeedbackCommonTest() {}

  scoped_refptr<FeedbackCommon> feedback;
  userfeedback::ExtensionSubmit report;
};

TEST_F(FeedbackCommonTest, TestBasicData) {
  // Test that basic data can be set and propagates to the request.
  feedback->set_category_tag(kOne);
  feedback->set_description(kTwo);
  feedback->set_page_url(kThree);
  feedback->set_user_email(kFour);
  feedback->PrepareReport(&report);

  EXPECT_EQ(kOne, report.bucket());
  EXPECT_EQ(kTwo, report.common_data().description());
  EXPECT_EQ(kThree, report.web_data().url());
  EXPECT_EQ(kFour, report.common_data().user_email());
}

TEST_F(FeedbackCommonTest, TestAddLogs) {
  feedback->AddLog(kOne, kTwo);
  feedback->AddLog(kThree, kFour);

  EXPECT_EQ(2U, feedback->sys_info()->size());
}

TEST_F(FeedbackCommonTest, TestCompressionThreshold) {
  // Add a large and small log, verify that only the small log gets
  // included in the report.
  feedback->AddLog(kOne, kTwo);
  feedback->AddLog(kThree, kLongLog);
  feedback->PrepareReport(&report);

  EXPECT_EQ(1, report.web_data().product_specific_data_size());
  EXPECT_EQ(kOne, report.web_data().product_specific_data(0).key());
}

TEST_F(FeedbackCommonTest, TestCompression) {
  // Add a large and small log, verify that an attachment has been
  // added with the right name.
  feedback->AddLog(kOne, kTwo);
  feedback->AddLog(kThree, kLongLog);
  feedback->CompressLogs();
  feedback->PrepareReport(&report);

  EXPECT_EQ(1, report.product_specific_binary_data_size());
  EXPECT_EQ(kLogsAttachmentName, report.product_specific_binary_data(0).name());
}
