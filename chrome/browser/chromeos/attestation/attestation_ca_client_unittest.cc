// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "content/public/test/test_browser_thread.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::StrictMock;

namespace chromeos {
namespace attestation {

class AttestationCAClientTest : public ::testing::Test {
 public:
  AttestationCAClientTest()
      : io_thread_(content::BrowserThread::IO, &message_loop_) {
  }

  virtual ~AttestationCAClientTest() {
  }

  MOCK_METHOD2(DataCallback, void(bool result, const std::string& data));

 protected:
  void SendResponse(net::URLRequestStatus::Status status, int response_code) {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    CHECK(fetcher);
    fetcher->set_status(net::URLRequestStatus(status, 0));
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(fetcher->upload_data() + "_response");
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  MessageLoop message_loop_;
  content::TestBrowserThread io_thread_;
  net::TestURLFetcherFactory url_fetcher_factory_;
};

TEST_F(AttestationCAClientTest, EnrollRequest) {
  AttestationCAClient client;
  EXPECT_CALL(*this, DataCallback(true, "enroll_response")).Times(1);
  client.SendEnrollRequest(
      "enroll",
      base::Bind(&AttestationCAClientTest::DataCallback,
                 base::Unretained(this)));
  SendResponse(net::URLRequestStatus::SUCCESS, net::HTTP_OK);
}

TEST_F(AttestationCAClientTest, CertificateRequest) {
  AttestationCAClient client;
  EXPECT_CALL(*this, DataCallback(true, "certificate_response")).Times(1);
  client.SendCertificateRequest(
      "certificate",
      base::Bind(&AttestationCAClientTest::DataCallback,
                 base::Unretained(this)));
  SendResponse(net::URLRequestStatus::SUCCESS, net::HTTP_OK);
}

TEST_F(AttestationCAClientTest, CertificateRequestNetworkFailure) {
  AttestationCAClient client;
  EXPECT_CALL(*this, DataCallback(false, "")).Times(1);
  client.SendCertificateRequest(
      "certificate",
      base::Bind(&AttestationCAClientTest::DataCallback,
                 base::Unretained(this)));
  SendResponse(net::URLRequestStatus::FAILED, net::HTTP_OK);
}

TEST_F(AttestationCAClientTest, CertificateRequestHttpError) {
  AttestationCAClient client;
  EXPECT_CALL(*this, DataCallback(false, "")).Times(1);
  client.SendCertificateRequest(
      "certificate",
      base::Bind(&AttestationCAClientTest::DataCallback,
                 base::Unretained(this)));
  SendResponse(net::URLRequestStatus::SUCCESS, net::HTTP_NOT_FOUND);
}

}  // namespace attestation
}  // namespace chromeos
