// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/ntp_background_service.h"

#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/search/background/ntp_background_data.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class NtpBackgroundServiceTest : public testing::Test {
 public:
  NtpBackgroundServiceTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  ~NtpBackgroundServiceTest() override {}

  void SetUp() override {
    testing::Test::SetUp();

    service_ = std::make_unique<NtpBackgroundService>(
        test_shared_loader_factory_, base::nullopt);
  }

  void SetUpResponseWithData(const std::string& response) {
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {}));
    test_url_loader_factory_.AddResponse(
        service_->GetLoadURLForTesting().spec(), response);
  }

  void SetUpResponseWithNetworkError() {
    test_url_loader_factory_.AddResponse(
        service_->GetLoadURLForTesting(), network::ResourceResponseHead(),
        std::string(), network::URLLoaderCompletionStatus(net::HTTP_NOT_FOUND));
  }

  NtpBackgroundService* service() { return service_.get(); }

 private:
  // Required to run tests from UI and threads.
  content::TestBrowserThreadBundle thread_bundle_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  std::unique_ptr<NtpBackgroundService> service_;
};

TEST_F(NtpBackgroundServiceTest, CollectionInfoNetworkError) {
  SetUpResponseWithNetworkError();

  ASSERT_TRUE(service()->collection_info().empty());

  base::RunLoop loop;
  loop.QuitWhenIdle();
  service()->FetchCollectionInfo();
  loop.Run();

  ASSERT_TRUE(service()->collection_info().empty());
}

TEST_F(NtpBackgroundServiceTest, BadCollectionsResponse) {
  SetUpResponseWithData("bad serialized GetCollectionsResponse");

  ASSERT_TRUE(service()->collection_info().empty());

  base::RunLoop loop;
  loop.QuitWhenIdle();
  service()->FetchCollectionInfo();
  loop.Run();

  EXPECT_TRUE(service()->collection_info().empty());
}

TEST_F(NtpBackgroundServiceTest, GoodCollectionsResponse) {
  ntp::background::Collection collection;
  collection.set_collection_id("shapes");
  collection.set_collection_name("Shapes");
  collection.add_preview()->set_image_url("image url");
  ntp::background::GetCollectionsResponse response;
  *response.add_collections() = collection;
  std::string response_string;
  response.SerializeToString(&response_string);

  SetUpResponseWithData(response_string);

  ASSERT_TRUE(service()->collection_info().empty());

  base::RunLoop loop;
  loop.QuitWhenIdle();
  service()->FetchCollectionInfo();
  loop.Run();

  CollectionInfo collection_info;
  collection_info.collection_id = collection.collection_id();
  collection_info.collection_name = collection.collection_name();
  collection_info.preview_image_url = GURL(collection.preview(0).image_url());

  EXPECT_FALSE(service()->collection_info().empty());
  EXPECT_THAT(service()->collection_info().at(0), testing::Eq(collection_info));
}
