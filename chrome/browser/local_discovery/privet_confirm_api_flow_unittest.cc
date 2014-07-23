// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privet_confirm_api_flow.h"

#include <set>

#include "base/json/json_reader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;
using testing::_;

namespace local_discovery {

namespace {

const char kSampleConfirmResponse[] = "{"
    "   \"success\": true"
    "}";

const char kFailedConfirmResponse[] = "{"
    "   \"success\": false"
    "}";

TEST(PrivetConfirmApiFlowTest, Params) {
  PrivetConfirmApiCallFlow confirmation(
      "123", PrivetConfirmApiCallFlow::ResponseCallback());
  EXPECT_EQ(GURL("https://www.google.com/cloudprint/confirm?token=123"),
            confirmation.GetURL());
  EXPECT_EQ("https://www.googleapis.com/auth/cloudprint",
            confirmation.GetOAuthScope());
  EXPECT_EQ(net::URLFetcher::GET, confirmation.GetRequestType());
  EXPECT_FALSE(confirmation.GetExtraRequestHeaders().empty());
}

class MockDelegate {
 public:
  MOCK_METHOD1(Callback, void(GCDApiFlow::Status));
};

TEST(PrivetConfirmApiFlowTest, Parsing) {
  StrictMock<MockDelegate> delegate;
  PrivetConfirmApiCallFlow confirmation(
      "123", base::Bind(&MockDelegate::Callback, base::Unretained(&delegate)));
  EXPECT_CALL(delegate, Callback(GCDApiFlow::SUCCESS)).Times(1);

  scoped_ptr<base::Value> value(base::JSONReader::Read(kSampleConfirmResponse));
  const base::DictionaryValue* dictionary = NULL;
  ASSERT_TRUE(value->GetAsDictionary(&dictionary));
  confirmation.OnGCDAPIFlowComplete(*dictionary);

  EXPECT_CALL(delegate, Callback(GCDApiFlow::ERROR_FROM_SERVER)).Times(1);

  value.reset(base::JSONReader::Read(kFailedConfirmResponse));
  ASSERT_TRUE(value->GetAsDictionary(&dictionary));
  confirmation.OnGCDAPIFlowComplete(*dictionary);
}

}  // namespace

}  // namespace local_discovery
