// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/metadata.pb.h"
#include "components/safe_browsing_db/safe_browsing_api_handler_util.h"
#include "components/safe_browsing_db/util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingApiHandlerUtilTest : public ::testing::Test {
 protected:
  SBThreatType threat_;
  std::string pb_str_;

  UmaRemoteCallResult ResetAndParseJson(const std::string& json) {
    threat_ = SB_THREAT_TYPE_EXTENSION;  // Should never be seen
    pb_str_ = "unitialized value";
    return ParseJsonToThreatAndPB(json, &threat_, &pb_str_);
  }

  MalwarePatternType::PATTERN_TYPE ParsePatternType() {
    MalwarePatternType proto;
    EXPECT_TRUE(proto.ParseFromString(pb_str_));
    return proto.pattern_type();
  }
};

TEST_F(SafeBrowsingApiHandlerUtilTest, BadJson) {
  EXPECT_EQ(UMA_STATUS_JSON_EMPTY, ResetAndParseJson(""));
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, threat_);
  EXPECT_TRUE(pb_str_.empty());

  EXPECT_EQ(UMA_STATUS_JSON_FAILED_TO_PARSE, ResetAndParseJson("{"));
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, threat_);
  EXPECT_TRUE(pb_str_.empty());

  EXPECT_EQ(UMA_STATUS_JSON_FAILED_TO_PARSE, ResetAndParseJson("[]"));
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, threat_);
  EXPECT_TRUE(pb_str_.empty());

  EXPECT_EQ(UMA_STATUS_JSON_FAILED_TO_PARSE,
            ResetAndParseJson("{\"matches\":\"foo\"}"));
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, threat_);
  EXPECT_TRUE(pb_str_.empty());

  EXPECT_EQ(UMA_STATUS_JSON_UNKNOWN_THREAT,
            ResetAndParseJson("{\"matches\":[{}]}"));
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, threat_);
  EXPECT_TRUE(pb_str_.empty());

  EXPECT_EQ(UMA_STATUS_JSON_UNKNOWN_THREAT,
            ResetAndParseJson("{\"matches\":[{\"threat_type\":\"junk\"}]}"));
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, threat_);
  EXPECT_TRUE(pb_str_.empty());

  EXPECT_EQ(UMA_STATUS_JSON_UNKNOWN_THREAT,
            ResetAndParseJson("{\"matches\":[{\"threat_type\":\"999\"}]}"));
  EXPECT_EQ(SB_THREAT_TYPE_SAFE, threat_);
  EXPECT_TRUE(pb_str_.empty());
}

TEST_F(SafeBrowsingApiHandlerUtilTest, BasicThreats) {
  EXPECT_EQ(UMA_STATUS_UNSAFE,
            ResetAndParseJson("{\"matches\":[{\"threat_type\":\"4\"}]}"));
  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, threat_);
  EXPECT_TRUE(pb_str_.empty());

  EXPECT_EQ(UMA_STATUS_UNSAFE,
            ResetAndParseJson("{\"matches\":[{\"threat_type\":\"5\"}]}"));
  EXPECT_EQ(SB_THREAT_TYPE_URL_PHISHING, threat_);
  EXPECT_TRUE(pb_str_.empty());
}

TEST_F(SafeBrowsingApiHandlerUtilTest, MultipleThreats) {
  EXPECT_EQ(
      UMA_STATUS_UNSAFE,
      ResetAndParseJson(
          "{\"matches\":[{\"threat_type\":\"4\"}, {\"threat_type\":\"5\"}]}"));
  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, threat_);
  EXPECT_TRUE(pb_str_.empty());
}

TEST_F(SafeBrowsingApiHandlerUtilTest, PhaSubType) {
  EXPECT_EQ(UMA_STATUS_UNSAFE,
            ResetAndParseJson("{\"matches\":[{\"threat_type\":\"4\", "
                              "\"pha_pattern_type\":\"LANDING\"}]}"));
  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, threat_);
  EXPECT_EQ(MalwarePatternType::LANDING, ParsePatternType());

  EXPECT_EQ(UMA_STATUS_UNSAFE,
            ResetAndParseJson("{\"matches\":[{\"threat_type\":\"4\", "
                              "\"pha_pattern_type\":\"DISTRIBUTION\"}]}"));
  EXPECT_EQ(SB_THREAT_TYPE_URL_MALWARE, threat_);
  EXPECT_EQ(MalwarePatternType::DISTRIBUTION, ParsePatternType());

  EXPECT_EQ(UMA_STATUS_UNSAFE,
            ResetAndParseJson("{\"matches\":[{\"threat_type\":\"4\", "
                              "\"pha_pattern_type\":\"junk\"}]}"));
  EXPECT_TRUE(pb_str_.empty());
}

TEST_F(SafeBrowsingApiHandlerUtilTest, SocialEngineeringSubType) {
  EXPECT_EQ(UMA_STATUS_UNSAFE,
            ResetAndParseJson("{\"matches\":[{\"threat_type\":\"5\", "
                              "\"se_pattern_type\":\"LANDING\"}]}"));
  EXPECT_EQ(SB_THREAT_TYPE_URL_PHISHING, threat_);
  EXPECT_EQ(MalwarePatternType::LANDING, ParsePatternType());

  EXPECT_EQ(UMA_STATUS_UNSAFE,
            ResetAndParseJson("{\"matches\":[{\"threat_type\":\"5\", "
                              "\"se_pattern_type\":\"DISTRIBUTION\"}]}"));
  EXPECT_EQ(SB_THREAT_TYPE_URL_PHISHING, threat_);
  EXPECT_EQ(MalwarePatternType::DISTRIBUTION, ParsePatternType());

  EXPECT_EQ(UMA_STATUS_UNSAFE,
            ResetAndParseJson("{\"matches\":[{\"threat_type\":\"5\", "
                              "\"se_pattern_type\":\"junk\"}]}"));
  EXPECT_TRUE(pb_str_.empty());
}

}  // namespace safe_browsing
