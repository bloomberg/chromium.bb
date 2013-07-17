// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/chromeos/attestation/attestation_ca_client.h"
#include "content/public/test/test_browser_thread.h"
#include "net/http/http_status_code.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace attestation {

class AttestationCAClientTest : public ::testing::Test {
 public:
  AttestationCAClientTest()
      : io_thread_(content::BrowserThread::IO, &message_loop_),
        num_invocations_(0),
        result_(false) {
  }

  virtual ~AttestationCAClientTest() {
  }

  void DataCallback (bool result, const std::string& data) {
    ++num_invocations_;
    result_ = result;
    data_ = data;
  }

  void DeleteClientDataCallback (AttestationCAClient* client,
                                 bool result,
                                 const std::string& data) {
    delete client;
    DataCallback(result, data);
  }

 protected:
  void SendResponse(net::URLRequestStatus::Status status, int response_code) {
    net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
    CHECK(fetcher);
    fetcher->set_status(net::URLRequestStatus(status, 0));
    fetcher->set_response_code(response_code);
    fetcher->SetResponseString(fetcher->upload_data() + "_response");
    fetcher->delegate()->OnURLFetchComplete(fetcher);
  }

  base::MessageLoop message_loop_;
  content::TestBrowserThread io_thread_;
  net::TestURLFetcherFactory url_fetcher_factory_;

  // For use with DataCallback.
  int num_invocations_;
  bool result_;
  std::string data_;
};

TEST_F(AttestationCAClientTest, EnrollRequest) {
  AttestationCAClient client;
  client.SendEnrollRequest(
      "enroll",
      base::Bind(&AttestationCAClientTest::DataCallback,
                 base::Unretained(this)));
  SendResponse(net::URLRequestStatus::SUCCESS, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_TRUE(result_);
  EXPECT_EQ("enroll_response", data_);
}

TEST_F(AttestationCAClientTest, CertificateRequest) {
  AttestationCAClient client;
  client.SendCertificateRequest(
      "certificate",
      base::Bind(&AttestationCAClientTest::DataCallback,
                 base::Unretained(this)));
  SendResponse(net::URLRequestStatus::SUCCESS, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_TRUE(result_);
  EXPECT_EQ("certificate_response", data_);
}

TEST_F(AttestationCAClientTest, CertificateRequestNetworkFailure) {
  AttestationCAClient client;
  client.SendCertificateRequest(
      "certificate",
      base::Bind(&AttestationCAClientTest::DataCallback,
                 base::Unretained(this)));
  SendResponse(net::URLRequestStatus::FAILED, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_FALSE(result_);
  EXPECT_EQ("", data_);
}

TEST_F(AttestationCAClientTest, CertificateRequestHttpError) {
  AttestationCAClient client;
  client.SendCertificateRequest(
      "certificate",
      base::Bind(&AttestationCAClientTest::DataCallback,
                 base::Unretained(this)));
  SendResponse(net::URLRequestStatus::SUCCESS, net::HTTP_NOT_FOUND);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_FALSE(result_);
  EXPECT_EQ("", data_);
}

TEST_F(AttestationCAClientTest, DeleteOnCallback) {
  AttestationCAClient* client = new AttestationCAClient();
  client->SendCertificateRequest(
      "certificate",
      base::Bind(&AttestationCAClientTest::DeleteClientDataCallback,
                 base::Unretained(this),
                 client));
  SendResponse(net::URLRequestStatus::SUCCESS, net::HTTP_OK);

  EXPECT_EQ(1, num_invocations_);
  EXPECT_TRUE(result_);
  EXPECT_EQ("certificate_response", data_);
}

}  // namespace attestation
}  // namespace chromeos
