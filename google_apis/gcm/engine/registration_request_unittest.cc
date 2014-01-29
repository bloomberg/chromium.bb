// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "google_apis/gcm/engine/registration_request.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {
const uint64 kAndroidId = 42UL;
const char kCert[] = "0DEADBEEF420";
const char kDeveloperId[] = "Project1";
const char kLoginHeader[] = "AidLogin";
const char kNoRegistrationIdReturned[] = "No Registration Id returned";
const char kAppId[] = "TestAppId";
const uint64 kSecurityToken = 77UL;
const uint64 kUserAndroidId = 1111;
const int64 kUserSerialNumber = 1;
}  // namespace

class RegistrationRequestTest : public testing::Test {
 public:
  RegistrationRequestTest();
  virtual ~RegistrationRequestTest();

  void RegistrationCallback(const std::string& registration_id);

  void CreateRequest(const std::string& sender_ids);
  void SetResponseStatusAndString(net::HttpStatusCode status_code,
                                  const std::string& response_body);
  void CompleteFetch();

 protected:
  std::string registration_id_;
  std::map<std::string, std::string> extras_;
  scoped_ptr<RegistrationRequest> request_;
  base::MessageLoop message_loop_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  scoped_refptr<net::TestURLRequestContextGetter> url_request_context_getter_;
};

RegistrationRequestTest::RegistrationRequestTest()
    : url_request_context_getter_(new net::TestURLRequestContextGetter(
          message_loop_.message_loop_proxy())) {}

RegistrationRequestTest::~RegistrationRequestTest() {}

void RegistrationRequestTest::RegistrationCallback(
    const std::string& registration_id) {
  registration_id_ = registration_id;
}

void RegistrationRequestTest::CreateRequest(const std::string& sender_ids) {
  std::vector<std::string> senders;
  base::StringTokenizer tokenizer(sender_ids, ",");
  while (tokenizer.GetNext())
    senders.push_back(tokenizer.token());

  request_.reset(new RegistrationRequest(
      RegistrationRequest::RequestInfo(kAndroidId,
                                       kSecurityToken,
                                       kUserAndroidId,
                                       kUserSerialNumber,
                                       kAppId,
                                       kCert,
                                       senders),
      base::Bind(&RegistrationRequestTest::RegistrationCallback,
                 base::Unretained(this)),
      url_request_context_getter_.get()));
}

void RegistrationRequestTest::SetResponseStatusAndString(
    net::HttpStatusCode status_code,
    const std::string& response_body) {
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->set_response_code(status_code);
  fetcher->SetResponseString(response_body);
}

void RegistrationRequestTest::CompleteFetch() {
  registration_id_ = kNoRegistrationIdReturned;
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);
  fetcher->delegate()->OnURLFetchComplete(fetcher);
}

TEST_F(RegistrationRequestTest, RequestDataPassedToFetcher) {
  CreateRequest(kDeveloperId);
  request_->Start();

  // Get data sent by request.
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  // Verify that authorization header was put together properly.
  net::HttpRequestHeaders headers;
  fetcher->GetExtraRequestHeaders(&headers);
  std::string auth_header;
  headers.GetHeader(net::HttpRequestHeaders::kAuthorization, &auth_header);
  base::StringTokenizer auth_tokenizer(auth_header, " :");
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(kLoginHeader, auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::Uint64ToString(kAndroidId), auth_tokenizer.token());
  ASSERT_TRUE(auth_tokenizer.GetNext());
  EXPECT_EQ(base::Uint64ToString(kSecurityToken), auth_tokenizer.token());

  std::map<std::string, std::string> expected_pairs;
  expected_pairs["app"] = kAppId;
  expected_pairs["sender"] = kDeveloperId;
  expected_pairs["cert"] = kCert;
  expected_pairs["device"] = base::Uint64ToString(kAndroidId);
  expected_pairs["device_user_id"] = base::Int64ToString(kUserSerialNumber);
  expected_pairs["X-GOOG.USER_AID"] = base::Uint64ToString(kUserAndroidId);

  // Verify data was formatted properly.
  std::string upload_data = fetcher->upload_data();
  base::StringTokenizer data_tokenizer(upload_data, "&=");
  while (data_tokenizer.GetNext()) {
    std::map<std::string, std::string>::iterator iter =
        expected_pairs.find(data_tokenizer.token());
    ASSERT_TRUE(iter != expected_pairs.end());
    ASSERT_TRUE(data_tokenizer.GetNext());
    EXPECT_EQ(iter->second, data_tokenizer.token());
    // Ensure that none of the keys appears twice.
    expected_pairs.erase(iter);
  }

  EXPECT_EQ(0UL, expected_pairs.size());
}

TEST_F(RegistrationRequestTest, RequestRegistrationWithMultipleSenderIds) {
  CreateRequest("sender1,sender2@gmail.com");
  request_->Start();

  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  ASSERT_TRUE(fetcher);

  // Verify data was formatted properly.
  std::string upload_data = fetcher->upload_data();
  base::StringTokenizer data_tokenizer(upload_data, "&=");

  // Skip all tokens until you hit entry for senders.
  while (data_tokenizer.GetNext() && data_tokenizer.token() != "sender")
    continue;

  ASSERT_TRUE(data_tokenizer.GetNext());
  std::string senders(net::UnescapeURLComponent(data_tokenizer.token(),
      net::UnescapeRule::URL_SPECIAL_CHARS));
  base::StringTokenizer sender_tokenizer(senders, ",");
  ASSERT_TRUE(sender_tokenizer.GetNext());
  EXPECT_EQ("sender1", sender_tokenizer.token());
  ASSERT_TRUE(sender_tokenizer.GetNext());
  EXPECT_EQ("sender2@gmail.com", sender_tokenizer.token());
}

TEST_F(RegistrationRequestTest, ResponseParsing) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "token=2501");
  CompleteFetch();

  EXPECT_EQ("2501", registration_id_);
}

TEST_F(RegistrationRequestTest, ResponseHttpStatusNotOK) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_UNAUTHORIZED, "token=2501");
  CompleteFetch();

  EXPECT_EQ("", registration_id_);
}

TEST_F(RegistrationRequestTest, ResponseMissingRegistrationId) {
  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "");
  CompleteFetch();

  EXPECT_EQ("", registration_id_);

  CreateRequest("sender1,sender2");
  request_->Start();

  SetResponseStatusAndString(net::HTTP_OK, "some error in response");
  CompleteFetch();

  EXPECT_EQ("", registration_id_);
}

}  // namespace gcm
