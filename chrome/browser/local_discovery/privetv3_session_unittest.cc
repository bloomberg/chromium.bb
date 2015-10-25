// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privetv3_session.h"

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/local_discovery/privet_http.h"
#include "content/public/test/test_utils.h"
#include "crypto/hmac.h"
#include "crypto/p224_spake.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_discovery {

namespace {

using testing::_;
using testing::ElementsAreArray;
using testing::Field;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SaveArg;
using testing::StrictMock;

using PairingType = PrivetV3Session::PairingType;
using Result = PrivetV3Session::Result;

const char kInfoResponse[] =
    "{\"version\":\"3.0\","
    "\"endpoints\":{\"httpsPort\": 443},"
    "\"authentication\":{"
    "  \"mode\":[\"anonymous\",\"pairing\",\"cloud\"],"
    "  \"pairing\":[\"pinCode\",\"embeddedCode\"],"
    "  \"crypto\":[\"p224_spake2\"]"
    "}}";

class MockPrivetHTTPClient : public PrivetHTTPClient {
 public:
  MockPrivetHTTPClient() {
    request_context_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
  }

  MOCK_METHOD0(GetName, const std::string&());
  MOCK_METHOD1(
      CreateInfoOperationPtr,
      PrivetJSONOperation*(const PrivetJSONOperation::ResultCallback&));
  MOCK_METHOD2(SwitchToHttps, void(uint16_t, const net::SHA256HashValue&));
  MOCK_CONST_METHOD0(IsInHttpsMode, bool());

  void RefreshPrivetToken(
      const PrivetURLFetcher::TokenCallback& callback) override {
    FAIL();
  }

  scoped_ptr<PrivetJSONOperation> CreateInfoOperation(
      const PrivetJSONOperation::ResultCallback& callback) override {
    return make_scoped_ptr(CreateInfoOperationPtr(callback));
  }

  scoped_ptr<PrivetURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcher::RequestType request_type,
      PrivetURLFetcher::Delegate* delegate) override {
    return make_scoped_ptr(new PrivetURLFetcher(
        url, request_type, request_context_.get(), delegate));
  }

  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
};

}  // namespace

class PrivetV3SessionTest : public testing::Test {
 public:
  PrivetV3SessionTest()
      : fetcher_factory_(nullptr),
        http_client_(new StrictMock<MockPrivetHTTPClient>()),
        session_(make_scoped_ptr(http_client_)) {}

  void OnInitialized(Result result, const base::DictionaryValue& info) {
    info_.MergeDictionary(&info);
    OnInitializedMock(result, info);
  }

  MOCK_METHOD2(OnInitializedMock, void(Result, const base::DictionaryValue&));
  MOCK_METHOD1(OnPairingStarted, void(Result));
  MOCK_METHOD1(OnCodeConfirmed, void(Result));
  MOCK_METHOD2(OnMessageSend, void(Result, const base::DictionaryValue&));
  MOCK_METHOD1(OnPostData, void(const base::DictionaryValue&));

 protected:
  void SetUp() override {
    session_.on_post_data_ =
        base::Bind(&PrivetV3SessionTest::OnPostData, base::Unretained(this));

    EXPECT_CALL(*http_client_, IsInHttpsMode()).WillRepeatedly(Return(false));
  }

  base::MessageLoop loop_;
  net::FakeURLFetcherFactory fetcher_factory_;
  StrictMock<MockPrivetHTTPClient>* http_client_;
  base::DictionaryValue info_;
  base::Closure quit_closure_;
  PrivetV3Session session_;
};

TEST_F(PrivetV3SessionTest, InitError) {
  EXPECT_CALL(*this, OnInitializedMock(Result::STATUS_CONNECTIONERROR, _))
      .Times(1);
  fetcher_factory_.SetFakeResponse(GURL("http://host/privet/info"), "",
                                   net::HTTP_OK, net::URLRequestStatus::FAILED);
  session_.Init(
      base::Bind(&PrivetV3SessionTest::OnInitialized, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PrivetV3SessionTest, VersionError) {
  std::string response(kInfoResponse);
  base::ReplaceFirstSubstringAfterOffset(&response, 0, "3.0", "4.1");

  EXPECT_CALL(*this, OnInitializedMock(Result::STATUS_SESSIONERROR, _))
      .Times(1);
  fetcher_factory_.SetFakeResponse(GURL("http://host/privet/info"), response,
                                   net::HTTP_OK,
                                   net::URLRequestStatus::SUCCESS);
  session_.Init(
      base::Bind(&PrivetV3SessionTest::OnInitialized, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PrivetV3SessionTest, ModeError) {
  std::string response(kInfoResponse);
  base::ReplaceFirstSubstringAfterOffset(&response, 0, "mode", "mode_");

  EXPECT_CALL(*this, OnInitializedMock(Result::STATUS_SESSIONERROR, _))
      .Times(1);
  fetcher_factory_.SetFakeResponse(GURL("http://host/privet/info"), response,
                                   net::HTTP_OK,
                                   net::URLRequestStatus::SUCCESS);
  session_.Init(
      base::Bind(&PrivetV3SessionTest::OnInitialized, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PrivetV3SessionTest, NoHttpsError) {
  EXPECT_CALL(*this, OnInitializedMock(Result::STATUS_SUCCESS, _)).Times(1);
  fetcher_factory_.SetFakeResponse(GURL("http://host/privet/info"),
                                   kInfoResponse, net::HTTP_OK,
                                   net::URLRequestStatus::SUCCESS);

  session_.Init(
      base::Bind(&PrivetV3SessionTest::OnInitialized, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*this, OnMessageSend(Result::STATUS_SESSIONERROR, _)).Times(1);
  session_.SendMessage(
      "/privet/v3/state", base::DictionaryValue(),
      base::Bind(&PrivetV3SessionTest::OnMessageSend, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();
}

TEST_F(PrivetV3SessionTest, Pairing) {
  EXPECT_CALL(*this, OnInitializedMock(Result::STATUS_SUCCESS, _))
      .Times(1);
  fetcher_factory_.SetFakeResponse(GURL("http://host/privet/info"),
                                   kInfoResponse, net::HTTP_OK,
                                   net::URLRequestStatus::SUCCESS);

  session_.Init(
      base::Bind(&PrivetV3SessionTest::OnInitialized, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  const base::ListValue* pairing = nullptr;
  ASSERT_TRUE(info_.GetList("authentication.pairing", &pairing));

  std::string pairing_string;
  ASSERT_TRUE(pairing->GetString(0, &pairing_string));
  EXPECT_EQ("pinCode", pairing_string);

  ASSERT_TRUE(pairing->GetString(1, &pairing_string));
  EXPECT_EQ("embeddedCode", pairing_string);

  crypto::P224EncryptedKeyExchange spake(
      crypto::P224EncryptedKeyExchange::kPeerTypeServer, "testPin");

  EXPECT_CALL(*this, OnPairingStarted(Result::STATUS_SUCCESS)).Times(1);
  EXPECT_CALL(*this, OnPostData(_))
      .WillOnce(
          Invoke([this, &spake](const base::DictionaryValue& data) {
            std::string pairing_type;
            EXPECT_TRUE(data.GetString("pairing", &pairing_type));
            EXPECT_EQ("embeddedCode", pairing_type);

            std::string crypto_type;
            EXPECT_TRUE(data.GetString("crypto", &crypto_type));
            EXPECT_EQ("p224_spake2", crypto_type);

            std::string device_commitment;
            base::Base64Encode(spake.GetNextMessage(), &device_commitment);
            fetcher_factory_.SetFakeResponse(
                GURL("http://host/privet/v3/pairing/start"),
                base::StringPrintf(
                    "{\"deviceCommitment\":\"%s\",\"sessionId\":\"testId\"}",
                    device_commitment.c_str()),
                net::HTTP_OK, net::URLRequestStatus::SUCCESS);
          }));
  session_.StartPairing(PairingType::PAIRING_TYPE_EMBEDDEDCODE,
                        base::Bind(&PrivetV3SessionTest::OnPairingStarted,
                                   base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("Privet anonymous", session_.privet_auth_token_);

  std::string fingerprint("testFingerprint  testFingerprint");
  net::SHA256HashValue sha_fingerprint;
  ASSERT_EQ(sizeof(sha_fingerprint.data), fingerprint.size());
  memcpy(sha_fingerprint.data, fingerprint.data(), fingerprint.size());

  EXPECT_CALL(*http_client_,
              SwitchToHttps(443, Field(&net::SHA256HashValue::data,
                                       ElementsAreArray(sha_fingerprint.data))))
      .WillOnce(InvokeWithoutArgs([this]() {
        EXPECT_CALL(*http_client_, IsInHttpsMode())
            .WillRepeatedly(Return(true));
      }));

  EXPECT_CALL(*this, OnCodeConfirmed(Result::STATUS_SUCCESS)).Times(1);
  EXPECT_CALL(*this, OnPostData(_))
      .WillOnce(Invoke(
          [this, &spake, fingerprint](const base::DictionaryValue& data) {
            std::string commitment_base64;
            EXPECT_TRUE(data.GetString("clientCommitment", &commitment_base64));
            std::string commitment;
            EXPECT_TRUE(base::Base64Decode(commitment_base64, &commitment));

            std::string session_id;
            EXPECT_TRUE(data.GetString("sessionId", &session_id));
            EXPECT_EQ("testId", session_id);

            EXPECT_EQ(spake.ProcessMessage(commitment),
                      crypto::P224EncryptedKeyExchange::kResultPending);

            std::string fingerprint_base64;
            base::Base64Encode(fingerprint, &fingerprint_base64);

            crypto::HMAC hmac(crypto::HMAC::SHA256);
            const std::string& key = spake.GetUnverifiedKey();
            EXPECT_TRUE(hmac.Init(key));
            std::string signature(hmac.DigestLength(), ' ');
            EXPECT_TRUE(hmac.Sign(fingerprint, reinterpret_cast<unsigned char*>(
                                                   string_as_array(&signature)),
                                  signature.size()));

            std::string signature_base64;
            base::Base64Encode(signature, &signature_base64);

            fetcher_factory_.SetFakeResponse(
                GURL("http://host/privet/v3/pairing/confirm"),
                base::StringPrintf(
                    "{\"certFingerprint\":\"%s\",\"certSignature\":\"%s\"}",
                    fingerprint_base64.c_str(), signature_base64.c_str()),
                net::HTTP_OK, net::URLRequestStatus::SUCCESS);
          }))
      .WillOnce(
          Invoke([this, &spake](const base::DictionaryValue& data) {
            std::string access_token_base64;
            EXPECT_TRUE(data.GetString("authCode", &access_token_base64));
            std::string access_token;
            EXPECT_TRUE(base::Base64Decode(access_token_base64, &access_token));

            crypto::HMAC hmac(crypto::HMAC::SHA256);
            const std::string& key = spake.GetUnverifiedKey();
            EXPECT_TRUE(hmac.Init(key));
            EXPECT_TRUE(hmac.Verify("testId", access_token));

            fetcher_factory_.SetFakeResponse(
                GURL("http://host/privet/v3/auth"),
                "{\"accessToken\":\"567\",\"tokenType\":\"testType\","
                "\"scope\":\"owner\"}",
                net::HTTP_OK, net::URLRequestStatus::SUCCESS);
          }));
  session_.ConfirmCode("testPin",
                       base::Bind(&PrivetV3SessionTest::OnCodeConfirmed,
                                  base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(session_.client_->IsInHttpsMode());
  EXPECT_EQ("testType 567", session_.privet_auth_token_);
}

TEST_F(PrivetV3SessionTest, Cancel) {
  EXPECT_CALL(*this, OnInitializedMock(Result::STATUS_SUCCESS, _)).Times(1);;
  fetcher_factory_.SetFakeResponse(GURL("http://host/privet/info"),
                                   kInfoResponse, net::HTTP_OK,
                                   net::URLRequestStatus::SUCCESS);

  session_.Init(
      base::Bind(&PrivetV3SessionTest::OnInitialized, base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*http_client_, IsInHttpsMode()).WillRepeatedly(Return(false));

  EXPECT_CALL(*this, OnPairingStarted(Result::STATUS_SUCCESS)).Times(1);
  EXPECT_CALL(*this, OnPostData(_))
      .WillOnce(Invoke([this](const base::DictionaryValue& data) {
        std::string device_commitment;
        base::Base64Encode("1234", &device_commitment);
        fetcher_factory_.SetFakeResponse(
            GURL("http://host/privet/v3/pairing/start"),
            base::StringPrintf(
                "{\"deviceCommitment\":\"%s\",\"sessionId\":\"testId\"}",
                device_commitment.c_str()),
            net::HTTP_OK, net::URLRequestStatus::SUCCESS);
      }));
  session_.StartPairing(PairingType::PAIRING_TYPE_EMBEDDEDCODE,
                        base::Bind(&PrivetV3SessionTest::OnPairingStarted,
                                   base::Unretained(this)));
  base::RunLoop().RunUntilIdle();

  fetcher_factory_.SetFakeResponse(GURL("http://host/privet/v3/pairing/cancel"),
                                   kInfoResponse, net::HTTP_OK,
                                   net::URLRequestStatus::SUCCESS);
  EXPECT_CALL(*this, OnPostData(_))
      .WillOnce(Invoke([this](const base::DictionaryValue& data) {
        std::string session_id;
        EXPECT_TRUE(data.GetString("sessionId", &session_id));
      }));
}

// TODO(vitalybuka): replace PrivetHTTPClient with regular URL fetcher and
// implement SendMessage test.

}  // namespace local_discovery
