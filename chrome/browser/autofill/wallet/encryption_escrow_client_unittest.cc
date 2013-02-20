// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/encryption_escrow_client.h"
#include "chrome/browser/autofill/wallet/encryption_escrow_client_observer.h"
#include "chrome/browser/autofill/wallet/instrument.h"
#include "chrome/browser/autofill/wallet/wallet_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kEncryptOtpRequest[] = "cvv=30:000102030405";
const char kEncryptOtpResponse[] = "session_material|encrypted_one_time_pad";
const char kEscrowSensitiveInformationRequest[] =
    "gid=obfuscated_gaia_id&cardNumber=4444444444444448&cvv=123";

}  // namespace

namespace autofill {
namespace wallet {

class MockEncryptionEscrowClientObserver :
  public EncryptionEscrowClientObserver,
  public base::SupportsWeakPtr<MockEncryptionEscrowClientObserver> {
 public:
  MockEncryptionEscrowClientObserver() {}
  ~MockEncryptionEscrowClientObserver() {}

  MOCK_METHOD2(OnDidEncryptOneTimePad,
               void(const std::string& encrypted_one_time_pad,
                    const std::string& session_material));
  MOCK_METHOD1(OnDidEscrowSensitiveInformation,
               void(const std::string& escrow_handle));
  MOCK_METHOD0(OnMalformedResponse, void());
  MOCK_METHOD1(OnNetworkError, void(int response_code));
};

class EncryptionEscrowClientTest : public testing::Test {
 public:
  EncryptionEscrowClientTest() : io_thread_(content::BrowserThread::IO) {}

  virtual void SetUp() {
    io_thread_.StartIOThread();
    profile_.CreateRequestContext();
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

  virtual void TearDown() {
    profile_.ResetRequestContext();
    io_thread_.Stop();
  }

  void VerifyAndFinishRequest(const net::TestURLFetcherFactory& fetcher_factory,
                              net::HttpStatusCode response_code,
                              const std::string& request_body,
                              const std::string& response_body) {
    net::TestURLFetcher* fetcher = fetcher_factory.GetFetcherByID(1);
    ASSERT_TRUE(fetcher);
    EXPECT_EQ(request_body, fetcher->upload_data());
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(response_body);
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

 protected:
  TestingProfile profile_;

 private:
  // The profile's request context must be released on the IO thread.
  content::TestBrowserThread io_thread_;
};

TEST_F(EncryptionEscrowClientTest, NetworkError) {
  MockEncryptionEscrowClientObserver observer;
  EXPECT_CALL(observer, OnNetworkError(net::HTTP_UNAUTHORIZED)).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  EncryptionEscrowClient encryption_escrow_client(profile_.GetRequestContext());
  encryption_escrow_client.EscrowSensitiveInformation(*instrument,
                                                      "obfuscated_gaia_id",
                                                      observer.AsWeakPtr());
  VerifyAndFinishRequest(factory,
                         net::HTTP_UNAUTHORIZED,
                         kEscrowSensitiveInformationRequest,
                         std::string());
}

TEST_F(EncryptionEscrowClientTest, EscrowSensitiveInformationSuccess) {
  MockEncryptionEscrowClientObserver observer;
  EXPECT_CALL(observer, OnDidEscrowSensitiveInformation("abc")).Times(1);

  net::TestURLFetcherFactory factory;

  scoped_ptr<Instrument> instrument = GetTestInstrument();
  EncryptionEscrowClient encryption_escrow_client(profile_.GetRequestContext());
  encryption_escrow_client.EscrowSensitiveInformation(*instrument,
                                                      "obfuscated_gaia_id",
                                                      observer.AsWeakPtr());
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kEscrowSensitiveInformationRequest,
                         "abc");
}

TEST_F(EncryptionEscrowClientTest, EscrowSensitiveInformationFailure) {
  MockEncryptionEscrowClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;
  scoped_ptr<Instrument> instrument = GetTestInstrument();
  EncryptionEscrowClient encryption_escrow_client(profile_.GetRequestContext());
  encryption_escrow_client.EscrowSensitiveInformation(*instrument,
                                                      "obfuscated_gaia_id",
                                                      observer.AsWeakPtr());
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kEscrowSensitiveInformationRequest,
                         std::string());
}

TEST_F(EncryptionEscrowClientTest, EncryptOneTimePadSuccess) {
  MockEncryptionEscrowClientObserver observer;
  EXPECT_CALL(observer,
              OnDidEncryptOneTimePad("encrypted_one_time_pad",
                                     "session_material")).Times(1);

  net::TestURLFetcherFactory factory;
  EncryptionEscrowClient encryption_escrow_client(profile_.GetRequestContext());
  encryption_escrow_client.EncryptOneTimePad(MakeOneTimePad(),
                                             observer.AsWeakPtr());
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kEncryptOtpRequest,
                         kEncryptOtpResponse);
}

TEST_F(EncryptionEscrowClientTest, EncryptOneTimePadFailure) {
  MockEncryptionEscrowClientObserver observer;
  EXPECT_CALL(observer, OnMalformedResponse()).Times(1);

  net::TestURLFetcherFactory factory;
  EncryptionEscrowClient encryption_escrow_client(profile_.GetRequestContext());
  encryption_escrow_client.EncryptOneTimePad(MakeOneTimePad(),
                                             observer.AsWeakPtr());
  VerifyAndFinishRequest(factory,
                         net::HTTP_OK,
                         kEncryptOtpRequest,
                         std::string());
}

}  // namespace wallet
}  // namespace autofill
