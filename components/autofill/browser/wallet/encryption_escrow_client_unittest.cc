// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/browser/wallet/encryption_escrow_client.h"
#include "components/autofill/browser/wallet/encryption_escrow_client_observer.h"
#include "components/autofill/browser/wallet/instrument.h"
#include "components/autofill/browser/wallet/wallet_test_util.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace wallet {

namespace {

const char kEncryptOtpRequest[] = "cvv=30:000102030405";
const char kEncryptOtpResponse[] = "session_material|encrypted_one_time_pad";
const char kEscrowInstrumentInformationRequest[] =
    "gid=obfuscated_gaia_id&cardNumber=4444444444444448&cvv=123";
const char kEscrowCardVerificationNumberRequest[] =
    "gid=obfuscated_gaia_id&cvv=123";

class MockEncryptionEscrowClientObserver :
    public EncryptionEscrowClientObserver,
    public base::SupportsWeakPtr<MockEncryptionEscrowClientObserver> {
 public:
  MockEncryptionEscrowClientObserver() {}
  ~MockEncryptionEscrowClientObserver() {}

  MOCK_METHOD2(OnDidEncryptOneTimePad,
               void(const std::string& encrypted_one_time_pad,
                    const std::string& session_material));
  MOCK_METHOD1(OnDidEscrowCardVerificationNumber,
               void(const std::string& escrow_handle));
  MOCK_METHOD1(OnDidEscrowInstrumentInformation,
               void(const std::string& escrow_handle));
  // TODO(isherman): Add test expectations for calls to this method.
  MOCK_METHOD0(OnDidMakeRequest, void());
  MOCK_METHOD0(OnMalformedResponse, void());
  MOCK_METHOD1(OnNetworkError, void(int response_code));
};

}  // namespace

class TestEncryptionEscrowClient : public EncryptionEscrowClient {
 public:
  TestEncryptionEscrowClient(
      net::URLRequestContextGetter* context_getter,
      EncryptionEscrowClientObserver* observer)
      : EncryptionEscrowClient(context_getter, observer) {}

  bool HasRequestInProgress() const {
    return request() != NULL;
  }
};

class EncryptionEscrowClientTest : public testing::Test {
 public:
  EncryptionEscrowClientTest()
      : instrument_(GetTestInstrument()),
        io_thread_(content::BrowserThread::IO) {}

  virtual void SetUp() OVERRIDE {
    io_thread_.StartIOThread();
    profile_.CreateRequestContext();
    encryption_escrow_client_.reset(new TestEncryptionEscrowClient(
        profile_.GetRequestContext(), &observer_));
  }

  virtual void TearDown() OVERRIDE {
    encryption_escrow_client_.reset();
    profile_.ResetRequestContext();
    io_thread_.Stop();
  }

  std::vector<uint8> MakeOneTimePad() {
    std::vector<uint8> one_time_pad;
    one_time_pad.push_back(0);
    one_time_pad.push_back(1);
    one_time_pad.push_back(2);
    one_time_pad.push_back(3);
    one_time_pad.push_back(4);
    one_time_pad.push_back(5);
    return one_time_pad;
  }

  void VerifyAndFinishRequest(net::HttpStatusCode response_code,
                              const std::string& request_body,
                              const std::string& response_body) {
    net::TestURLFetcher* fetcher = factory_.GetFetcherByID(1);
    ASSERT_TRUE(fetcher);
    EXPECT_EQ(request_body, fetcher->upload_data());
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response_body);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

 protected:
  scoped_ptr<TestEncryptionEscrowClient> encryption_escrow_client_;
  testing::StrictMock<MockEncryptionEscrowClientObserver> observer_;
  scoped_ptr<Instrument> instrument_;


 private:
  // The profile's request context must be released on the IO thread.
  content::TestBrowserThread io_thread_;
  TestingProfile profile_;
  net::TestURLFetcherFactory factory_;
};

TEST_F(EncryptionEscrowClientTest, NetworkError) {
  EXPECT_CALL(observer_, OnDidMakeRequest()).Times(1);
  EXPECT_CALL(observer_, OnNetworkError(net::HTTP_UNAUTHORIZED)).Times(1);

  encryption_escrow_client_->EscrowInstrumentInformation(*instrument_,
                                                         "obfuscated_gaia_id");
  VerifyAndFinishRequest(net::HTTP_UNAUTHORIZED,
                         kEscrowInstrumentInformationRequest,
                         std::string());
}

TEST_F(EncryptionEscrowClientTest, EscrowInstrumentInformationSuccess) {
  EXPECT_CALL(observer_, OnDidMakeRequest()).Times(1);
  EXPECT_CALL(observer_, OnDidEscrowInstrumentInformation("abc")).Times(1);

  encryption_escrow_client_->EscrowInstrumentInformation(*instrument_,
                                                         "obfuscated_gaia_id");
  VerifyAndFinishRequest(net::HTTP_OK,
                         kEscrowInstrumentInformationRequest,
                         "abc");
}

TEST_F(EncryptionEscrowClientTest, EscrowInstrumentInformationFailure) {
  EXPECT_CALL(observer_, OnDidMakeRequest()).Times(1);
  EXPECT_CALL(observer_, OnMalformedResponse()).Times(1);

  encryption_escrow_client_->EscrowInstrumentInformation(*instrument_,
                                                         "obfuscated_gaia_id");
  VerifyAndFinishRequest(net::HTTP_OK,
                         kEscrowInstrumentInformationRequest,
                         std::string());
}

TEST_F(EncryptionEscrowClientTest, EscrowCardVerificationNumberSuccess) {
  EXPECT_CALL(observer_, OnDidMakeRequest()).Times(1);
  EXPECT_CALL(observer_, OnDidEscrowCardVerificationNumber("abc")).Times(1);

  encryption_escrow_client_->EscrowCardVerificationNumber("123",
                                                          "obfuscated_gaia_id");
  VerifyAndFinishRequest(net::HTTP_OK,
                         kEscrowCardVerificationNumberRequest,
                         "abc");
}

TEST_F(EncryptionEscrowClientTest, EscrowCardVerificationNumberFailure) {
  EXPECT_CALL(observer_, OnDidMakeRequest()).Times(1);
  EXPECT_CALL(observer_, OnMalformedResponse()).Times(1);

  encryption_escrow_client_->EscrowCardVerificationNumber("123",
                                                          "obfuscated_gaia_id");
  VerifyAndFinishRequest(net::HTTP_OK,
                         kEscrowCardVerificationNumberRequest,
                         std::string());
}

TEST_F(EncryptionEscrowClientTest, EncryptOneTimePadSuccess) {
  EXPECT_CALL(observer_, OnDidMakeRequest()).Times(1);
  EXPECT_CALL(observer_,
              OnDidEncryptOneTimePad("encrypted_one_time_pad",
                                     "session_material")).Times(1);

  encryption_escrow_client_->EncryptOneTimePad(MakeOneTimePad());
  VerifyAndFinishRequest(net::HTTP_OK, kEncryptOtpRequest, kEncryptOtpResponse);
}

TEST_F(EncryptionEscrowClientTest, EncryptOneTimePadFailure) {
  EXPECT_CALL(observer_, OnDidMakeRequest()).Times(1);
  EXPECT_CALL(observer_, OnMalformedResponse()).Times(1);

  encryption_escrow_client_->EncryptOneTimePad(MakeOneTimePad());
  VerifyAndFinishRequest(net::HTTP_OK, kEncryptOtpRequest, std::string());
}

TEST_F(EncryptionEscrowClientTest, CancelRequest) {
  EXPECT_CALL(observer_, OnDidMakeRequest()).Times(1);

  encryption_escrow_client_->EncryptOneTimePad(MakeOneTimePad());
  EXPECT_TRUE(encryption_escrow_client_->HasRequestInProgress());

  encryption_escrow_client_->CancelRequest();
  EXPECT_FALSE(encryption_escrow_client_->HasRequestInProgress());
}

}  // namespace wallet
}  // namespace autofill
