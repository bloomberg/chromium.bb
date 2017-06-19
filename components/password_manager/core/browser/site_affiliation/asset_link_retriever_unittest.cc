// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/site_affiliation/asset_link_retriever.h"

#include <memory>

#include "base/test/scoped_task_environment.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {
namespace {

constexpr char kAssetLinkFile[] =
    "https://example.com/.well-known/assetlinks.json";

// A test URL fecther which is very cautious about not following the redirects.
class AssetLinksTestFetcher : public net::FakeURLFetcher {
 public:
  using FakeURLFetcher::FakeURLFetcher;

  ~AssetLinksTestFetcher() override { EXPECT_TRUE(stop_on_redirect_); }

  // FakeURLFetcher:
  void SetStopOnRedirect(bool stop_on_redirect) override {
    FakeURLFetcher::SetStopOnRedirect(stop_on_redirect);
    stop_on_redirect_ = stop_on_redirect;
  }

 private:
  bool stop_on_redirect_ = false;

  DISALLOW_COPY_AND_ASSIGN(AssetLinksTestFetcher);
};

std::unique_ptr<net::FakeURLFetcher> AssetLinksFetcherCreator(
    const GURL& url,
    net::URLFetcherDelegate* delegate,
    const std::string& response_data,
    net::HttpStatusCode response_code,
    net::URLRequestStatus::Status status) {
  return base::MakeUnique<AssetLinksTestFetcher>(url, delegate, response_data,
                                                 response_code, status);
}

class AssetLinkRetrieverTest : public testing::Test {
 public:
  AssetLinkRetrieverTest();

  net::TestURLRequestContextGetter* request_context() const {
    return request_context_.get();
  }

  net::FakeURLFetcherFactory& factory() { return factory_; }

  void RunUntilIdle() { scoped_task_environment_.RunUntilIdle(); }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::FakeURLFetcherFactory factory_;

  DISALLOW_COPY_AND_ASSIGN(AssetLinkRetrieverTest);
};

AssetLinkRetrieverTest::AssetLinkRetrieverTest()
    : request_context_(new net::TestURLRequestContextGetter(
          base::ThreadTaskRunnerHandle::Get())),
      factory_(nullptr, base::Bind(&AssetLinksFetcherCreator)) {}

// Load the asset links resource that isn't available.
TEST_F(AssetLinkRetrieverTest, LoadNonExistent) {
  scoped_refptr<AssetLinkRetriever> asset_link_retriever =
      base::MakeRefCounted<AssetLinkRetriever>(GURL(kAssetLinkFile));
  EXPECT_EQ(AssetLinkRetriever::State::INACTIVE, asset_link_retriever->state());

  factory().SetFakeResponse(GURL(kAssetLinkFile), std::string(),
                            net::HTTP_NOT_FOUND, net::URLRequestStatus::FAILED);
  asset_link_retriever->Start(request_context());
  EXPECT_EQ(AssetLinkRetriever::State::NETWORK_REQUEST,
            asset_link_retriever->state());

  RunUntilIdle();
  EXPECT_EQ(AssetLinkRetriever::State::FINISHED, asset_link_retriever->state());
  EXPECT_TRUE(asset_link_retriever->error());
}

// Load the asset links resource that replies with redirect. It should be
// treated as an error.
TEST_F(AssetLinkRetrieverTest, LoadRedirect) {
  scoped_refptr<AssetLinkRetriever> asset_link_retriever =
      base::MakeRefCounted<AssetLinkRetriever>(GURL(kAssetLinkFile));
  EXPECT_EQ(AssetLinkRetriever::State::INACTIVE, asset_link_retriever->state());

  factory().SetFakeResponse(GURL(kAssetLinkFile), std::string(),
                            net::HTTP_FOUND, net::URLRequestStatus::CANCELED);
  asset_link_retriever->Start(request_context());

  RunUntilIdle();
  EXPECT_EQ(AssetLinkRetriever::State::FINISHED, asset_link_retriever->state());
  EXPECT_TRUE(asset_link_retriever->error());
}

}  // namespace
}  // namespace password_manager
