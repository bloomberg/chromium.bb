// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/gcd_registration_ticket_request.h"

#include "base/json/json_reader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace local_discovery {

namespace {

const char kSampleResponse[] =
    "{"
    "\"kind\": \"clouddevices#registrationTicket\","
    "\"id\": \"SampleTicketID\","
    "\"deviceId\": \"SampleDeviceID\""
    "}";

const char kErrorResponse[] =
    "{"
    "\"kind\": \"clouddevices#error\""
    "}";

TEST(GCDRegistrationTicketRequestTest, Params) {
  GCDRegistrationTicketRequest::ResponseCallback null_callback;
  GCDRegistrationTicketRequest request(null_callback);

  EXPECT_EQ(
      GURL("https://www.googleapis.com/clouddevices/v1/registrationTickets"),
      request.GetURL());
  EXPECT_EQ("https://www.googleapis.com/auth/clouddevices",
            request.GetOAuthScope());
  EXPECT_EQ(net::URLFetcher::POST, request.GetRequestType());
  EXPECT_TRUE(request.GetExtraRequestHeaders().empty());
}

class MockDelegate {
 public:
  MOCK_METHOD2(Callback,
               void(const std::string& ticket_id,
                    const std::string& device_id));
};

TEST(GCDRegistrationTicketRequestTest, Parsing) {
  StrictMock<MockDelegate> delegate;
  GCDRegistrationTicketRequest request(
      base::Bind(&MockDelegate::Callback, base::Unretained(&delegate)));

  EXPECT_CALL(delegate, Callback("SampleTicketID", "SampleDeviceID"));

  scoped_ptr<base::Value> value(base::JSONReader::Read(kSampleResponse));
  const base::DictionaryValue* dictionary = NULL;
  ASSERT_TRUE(value->GetAsDictionary(&dictionary));
  request.OnGCDAPIFlowComplete(*dictionary);

  EXPECT_CALL(delegate, Callback("", ""));

  value.reset(base::JSONReader::Read(kErrorResponse));
  ASSERT_TRUE(value->GetAsDictionary(&dictionary));
  request.OnGCDAPIFlowComplete(*dictionary);
}

}  // namespace

}  // namespace local_discovery
