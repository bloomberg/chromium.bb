// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/prefetch_network_request_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"
#include "components/offline_pages/core/prefetch/get_operation_request.h"
#include "components/version_info/channel.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class PrefetchNetworkRequestFactoryTest : public testing::Test {
 public:
  PrefetchNetworkRequestFactoryTest();

  PrefetchNetworkRequestFactoryImpl* request_factory() {
    return request_factory_.get();
  }

 private:
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  std::unique_ptr<PrefetchNetworkRequestFactoryImpl> request_factory_;
};

PrefetchNetworkRequestFactoryTest::PrefetchNetworkRequestFactoryTest()
    : task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_),
      request_context_(new net::TestURLRequestContextGetter(task_runner_)) {
  request_factory_ = base::MakeUnique<PrefetchNetworkRequestFactoryImpl>(
      request_context_.get(), version_info::Channel::UNKNOWN, "a user agent");
}

TEST_F(PrefetchNetworkRequestFactoryTest, TestMakeGetOperationRequest) {
  // Query whether there is an operation to start with, before we make a
  // request.
  std::string operation_name = "an operation";
  GetOperationRequest* request =
      request_factory()->FindGetOperationRequestByName(operation_name);
  EXPECT_EQ(nullptr, request);

  // Then, make the request and ensure we can find it by name.
  request_factory()->MakeGetOperationRequest(operation_name,
                                             PrefetchRequestFinishedCallback());
  request = request_factory()->FindGetOperationRequestByName(operation_name);
  EXPECT_NE(nullptr, request);

  // Then check that a request is not found for another name (which was not
  // requested).
  std::string operation_name_2 = "another operation";
  GetOperationRequest* request_2 =
      request_factory()->FindGetOperationRequestByName(operation_name_2);
  EXPECT_EQ(nullptr, request_2);

  // Then make the second request.
  request_factory()->MakeGetOperationRequest(operation_name_2,
                                             PrefetchRequestFinishedCallback());

  // Query for the second request, ensure it is different than the first
  // request, and ensure it didn't change the first request.
  request_2 =
      request_factory()->FindGetOperationRequestByName(operation_name_2);
  EXPECT_NE(nullptr, request_2);
  EXPECT_EQ(request,
            request_factory()->FindGetOperationRequestByName(operation_name));
  EXPECT_NE(request, request_2);

  // Then overwrite the first request with a new one, and make sure it's
  // different.
  request_factory()->MakeGetOperationRequest(operation_name,
                                             PrefetchRequestFinishedCallback());
  EXPECT_NE(request,
            request_factory()->FindGetOperationRequestByName(operation_name));
}

TEST_F(PrefetchNetworkRequestFactoryTest, TestMakeGeneratePageBundleRequest) {
  std::vector<std::string> urls = {"example.com/1", "example.com/2"};
  std::string reg_id = "a registration id";

  GeneratePageBundleRequest* request =
      request_factory()->CurrentGeneratePageBundleRequest();
  EXPECT_EQ(nullptr, request);

  request_factory()->MakeGeneratePageBundleRequest(
      urls, reg_id, PrefetchRequestFinishedCallback());
  request = request_factory()->CurrentGeneratePageBundleRequest();
  EXPECT_NE(nullptr, request);

  urls = {"example.com/3"};
  request_factory()->MakeGeneratePageBundleRequest(
      urls, reg_id, PrefetchRequestFinishedCallback());
  EXPECT_NE(request, request_factory()->CurrentGeneratePageBundleRequest());
}

}  // namespace offline_pages
