// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_manifest_downloader.h"

#include "base/threading/thread_task_runner_handle.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class PaymentManifestDownloaderTest
    : public testing::Test,
      public PaymentManifestDownloader::Delegate {
 public:
  PaymentManifestDownloaderTest()
      : context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
        downloader_(context_, GURL("https://bobpay.com"), this) {
    downloader_.Download();
  }

  ~PaymentManifestDownloaderTest() override {}

  // PaymentManifestDownloader::Delegate
  MOCK_METHOD1(OnManifestDownloadSuccess, void(const std::string& content));
  MOCK_METHOD0(OnManifestDownloadFailure, void());

  net::TestURLFetcher* fetcher() { return factory_.GetFetcherByID(0); }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  net::TestURLFetcherFactory factory_;
  scoped_refptr<net::TestURLRequestContextGetter> context_;
  PaymentManifestDownloader downloader_;

  DISALLOW_COPY_AND_ASSIGN(PaymentManifestDownloaderTest);
};

TEST_F(PaymentManifestDownloaderTest, HttpHeadResponse404IsFailure) {
  fetcher()->set_response_code(404);

  EXPECT_CALL(*this, OnManifestDownloadFailure());

  fetcher()->delegate()->OnURLFetchComplete(fetcher());
}

TEST_F(PaymentManifestDownloaderTest, NoHttpHeadersIsFailure) {
  fetcher()->set_response_code(200);

  EXPECT_CALL(*this, OnManifestDownloadFailure());

  fetcher()->delegate()->OnURLFetchComplete(fetcher());
}

TEST_F(PaymentManifestDownloaderTest, EmptyHttpHeaderIsFailure) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  fetcher()->set_response_headers(headers);
  fetcher()->set_response_code(200);

  EXPECT_CALL(*this, OnManifestDownloadFailure());

  fetcher()->delegate()->OnURLFetchComplete(fetcher());
}

TEST_F(PaymentManifestDownloaderTest, EmptyHttpLinkHeaderIsFailure) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  headers->AddHeader("Link:");
  fetcher()->set_response_headers(headers);
  fetcher()->set_response_code(200);

  EXPECT_CALL(*this, OnManifestDownloadFailure());

  fetcher()->delegate()->OnURLFetchComplete(fetcher());
}

TEST_F(PaymentManifestDownloaderTest, NoRelInHttpLinkHeaderIsFailure) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  headers->AddHeader("Link: <manifest.json>");
  fetcher()->set_response_headers(headers);
  fetcher()->set_response_code(200);

  EXPECT_CALL(*this, OnManifestDownloadFailure());

  fetcher()->delegate()->OnURLFetchComplete(fetcher());
}

TEST_F(PaymentManifestDownloaderTest, NoUrlInHttpLinkHeaderIsFailure) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  headers->AddHeader("Link: rel=payment-method-manifest");
  fetcher()->set_response_headers(headers);
  fetcher()->set_response_code(200);

  EXPECT_CALL(*this, OnManifestDownloadFailure());

  fetcher()->delegate()->OnURLFetchComplete(fetcher());
}

TEST_F(PaymentManifestDownloaderTest, NoManifestRellInHttpLinkHeaderIsFailure) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  headers->AddHeader("Link: <manifest.json>; rel=web-app-manifest");
  fetcher()->set_response_headers(headers);
  fetcher()->set_response_code(200);

  EXPECT_CALL(*this, OnManifestDownloadFailure());

  fetcher()->delegate()->OnURLFetchComplete(fetcher());
}

TEST_F(PaymentManifestDownloaderTest, HttpGetResponse404IsFailure) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  headers->AddHeader("Link: <manifest.json>; rel=payment-method-manifest");
  fetcher()->set_response_headers(headers);
  fetcher()->set_response_code(200);
  fetcher()->delegate()->OnURLFetchComplete(fetcher());
  fetcher()->set_response_code(404);

  EXPECT_CALL(*this, OnManifestDownloadFailure());

  fetcher()->delegate()->OnURLFetchComplete(fetcher());
}

TEST_F(PaymentManifestDownloaderTest, EmptyHttpGetResponseIsFailure) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  headers->AddHeader("Link: <manifest.json>; rel=payment-method-manifest");
  fetcher()->set_response_headers(headers);
  fetcher()->set_response_code(200);
  fetcher()->delegate()->OnURLFetchComplete(fetcher());
  fetcher()->set_response_code(200);

  EXPECT_CALL(*this, OnManifestDownloadFailure());

  fetcher()->delegate()->OnURLFetchComplete(fetcher());
}

TEST_F(PaymentManifestDownloaderTest, NonEmptyHttpGetResponseIsSuccess) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  headers->AddHeader("Link: <manifest.json>; rel=payment-method-manifest");
  fetcher()->set_response_headers(headers);
  fetcher()->set_response_code(200);
  fetcher()->delegate()->OnURLFetchComplete(fetcher());
  fetcher()->SetResponseString("manifest content");
  fetcher()->set_response_code(200);

  EXPECT_CALL(*this, OnManifestDownloadSuccess("manifest content"));

  fetcher()->delegate()->OnURLFetchComplete(fetcher());
}

TEST_F(PaymentManifestDownloaderTest, RelativeHttpHeaderLinkUrl) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  headers->AddHeader("Link: <manifest.json>; rel=payment-method-manifest");
  fetcher()->set_response_headers(headers);
  fetcher()->set_response_code(200);
  fetcher()->delegate()->OnURLFetchComplete(fetcher());

  EXPECT_EQ("https://bobpay.com/manifest.json",
            fetcher()->GetOriginalURL().spec());
}

TEST_F(PaymentManifestDownloaderTest, AbsoluteHttpsHeaderLinkUrl) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  headers->AddHeader(
      "Link: <https://alicepay.com/manifest.json>; "
      "rel=payment-method-manifest");
  fetcher()->set_response_headers(headers);
  fetcher()->set_response_code(200);
  fetcher()->delegate()->OnURLFetchComplete(fetcher());

  EXPECT_EQ("https://alicepay.com/manifest.json",
            fetcher()->GetOriginalURL().spec());
}

TEST_F(PaymentManifestDownloaderTest, AbsoluteHttpHeaderLinkUrl) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(std::string()));
  headers->AddHeader(
      "Link: <http://alicepay.com/manifest.json>; "
      "rel=payment-method-manifest");
  fetcher()->set_response_headers(headers);
  fetcher()->set_response_code(200);

  EXPECT_CALL(*this, OnManifestDownloadFailure());

  fetcher()->delegate()->OnURLFetchComplete(fetcher());
}

}  // namespace
}  // namespace payments
