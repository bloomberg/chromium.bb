// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/contextual_suggestions_service.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ContextualSuggestionsServiceTest : public testing::Test {
 public:
  ContextualSuggestionsServiceTest()
      : mock_task_runner_(new base::TestMockTimeTaskRunner(
            base::TestMockTimeTaskRunner::Type::kBoundToThread)) {}

  scoped_refptr<network::SharedURLLoaderFactory> GetUrlLoaderFactory() {
    return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
        &test_url_loader_factory_);
  }

  void RunAndWait() { mock_task_runner_->FastForwardUntilNoTasksRemain(); }

  void OnRequestStart(std::unique_ptr<network::SimpleURLLoader> loader) {}

  void OnRequestComplete(const network::SimpleURLLoader* source,
                         std::unique_ptr<std::string> response_body) {}

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(ContextualSuggestionsServiceTest, EnsureAttachCookies) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      omnibox::kZeroSuggestRedirectToChrome);

  network::ResourceRequest resource_request;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& request) {
        resource_request = request;
      }));

  ContextualSuggestionsService service(nullptr /* identity_manager */,
                                       GetUrlLoaderFactory());
  AutocompleteInput input;
  base::Time visit_time;
  TemplateURLService template_url_service(nullptr, 0);
  service.CreateContextualSuggestionsRequest(
      "https://www.google.com/", visit_time, input, &template_url_service,
      base::BindOnce(&ContextualSuggestionsServiceTest::OnRequestStart,
                     base::Unretained(this)),
      base::BindOnce(&ContextualSuggestionsServiceTest::OnRequestComplete,
                     base::Unretained(this)));

  RunAndWait();
  EXPECT_TRUE(resource_request.attach_same_site_cookies);
  EXPECT_EQ(net::LOAD_DO_NOT_SAVE_COOKIES, resource_request.load_flags);
  EXPECT_EQ(resource_request.url, resource_request.site_for_cookies);
  const std::string kServiceUri = "https://www.google.com/complete/search";
  EXPECT_EQ(kServiceUri,
            resource_request.url.spec().substr(0, kServiceUri.size()));
}
