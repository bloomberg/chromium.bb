// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/content_resource_type_provider.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_network_delegate.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "content/public/browser/resource_request_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

#if defined(OS_ANDROID)
const Client kClient = Client::CHROME_ANDROID;
#elif defined(OS_IOS)
const Client kClient = Client::CHROME_IOS;
#elif defined(OS_MACOSX)
const Client kClient = Client::CHROME_MAC;
#elif defined(OS_CHROMEOS)
const Client kClient = Client::CHROME_CHROMEOS;
#elif defined(OS_LINUX)
const Client kClient = Client::CHROME_LINUX;
#elif defined(OS_WIN)
const Client kClient = Client::CHROME_WINDOWS;
#elif defined(OS_FREEBSD)
const Client kClient = Client::CHROME_FREEBSD;
#elif defined(OS_OPENBSD)
const Client kClient = Client::CHROME_OPENBSD;
#elif defined(OS_SOLARIS)
const Client kClient = Client::CHROME_SOLARIS;
#elif defined(OS_QNX)
const Client kClient = Client::CHROME_QNX;
#else
const Client kClient = Client::UNKNOWN;
#endif

}  // namespace

class ContentResourceProviderTest : public testing::Test {
 public:
  ContentResourceProviderTest()
      : context_(true), content_resource_type_provider_(nullptr) {
    test_context_ = DataReductionProxyTestContext::Builder()
                        .WithClient(kClient)
                        .WithURLRequestContext(&context_)
                        .Build();

    data_reduction_proxy_network_delegate_.reset(
        new DataReductionProxyNetworkDelegate(
            std::unique_ptr<net::NetworkDelegate>(
                new net::NetworkDelegateImpl()),
            test_context_->config(),
            test_context_->io_data()->request_options(),
            test_context_->configurator()));

    data_reduction_proxy_network_delegate_->InitIODataAndUMA(
        test_context_->io_data(), test_context_->io_data()->bypass_stats());

    context_.set_network_delegate(data_reduction_proxy_network_delegate_.get());
    context_.set_proxy_delegate(test_context_->io_data()->proxy_delegate());
    context_.Init();

    std::unique_ptr<data_reduction_proxy::ContentResourceTypeProvider>
        content_resource_type_provider(
            new data_reduction_proxy::ContentResourceTypeProvider());
    content_resource_type_provider_ = content_resource_type_provider.get();
    test_context_->io_data()->set_resource_type_provider(
        std::move(content_resource_type_provider));
  }

  void AllocateRequestInfoForTesting(net::URLRequest* request,
                                     content::ResourceType resource_type) {
    content::ResourceRequestInfo::AllocateForTesting(
        request, resource_type, NULL, -1, -1, -1,
        resource_type == content::RESOURCE_TYPE_MAIN_FRAME,
        false,  // parent_is_main_frame
        false,  // allow_download
        false,  // is_async
        false   // is_using_lofi
        );
  }

  std::unique_ptr<net::URLRequest> CreateRequestByType(
      const GURL& gurl,
      content::ResourceType resource_type) {
    std::unique_ptr<net::URLRequest> request =
        context_.CreateRequest(gurl, net::IDLE, &delegate_);
    AllocateRequestInfoForTesting(request.get(), resource_type);
    return request;
  }

  ResourceTypeProvider* content_resource_type_provider() const {
    return content_resource_type_provider_;
  }

 protected:
  base::MessageLoopForIO message_loop_;
  net::TestURLRequestContext context_;
  net::TestDelegate delegate_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
  std::unique_ptr<DataReductionProxyNetworkDelegate>
      data_reduction_proxy_network_delegate_;
  ResourceTypeProvider* content_resource_type_provider_;
};

// Tests that the resource content type was correctly computed, and was
// available to the data reduction proxy delegate.
TEST_F(ContentResourceProviderTest, SetAndGetContentResourceType) {
  const struct {
    GURL gurl;
    content::ResourceType resource_type;
    ResourceTypeProvider::ContentType expected_content_type;
  } tests[] = {
      {GURL("http://www.google.com/main-frame"),
       content::RESOURCE_TYPE_MAIN_FRAME,
       ResourceTypeProvider::CONTENT_TYPE_UNKNOWN},
      {GURL("http://www.google.com/sub-frame"),
       content::RESOURCE_TYPE_SUB_FRAME,
       ResourceTypeProvider::CONTENT_TYPE_UNKNOWN},
      {GURL("http://www.google.com/stylesheet"),
       content::RESOURCE_TYPE_STYLESHEET,
       ResourceTypeProvider::CONTENT_TYPE_UNKNOWN},
      {GURL("http://www.google.com/script"), content::RESOURCE_TYPE_SCRIPT,
       ResourceTypeProvider::CONTENT_TYPE_UNKNOWN},
      {GURL("http://www.google.com/image"), content::RESOURCE_TYPE_IMAGE,
       ResourceTypeProvider::CONTENT_TYPE_UNKNOWN},
      {GURL("http://www.google.com/font"), content::RESOURCE_TYPE_FONT_RESOURCE,
       ResourceTypeProvider::CONTENT_TYPE_UNKNOWN},
      {GURL("http://www.google.com/sub-resource"),
       content::RESOURCE_TYPE_SUB_RESOURCE,
       ResourceTypeProvider::CONTENT_TYPE_UNKNOWN},
      {GURL("http://www.google.com/object"), content::RESOURCE_TYPE_OBJECT,
       ResourceTypeProvider::CONTENT_TYPE_UNKNOWN},
      {GURL("http://www.google.com/media"), content::RESOURCE_TYPE_MEDIA,
       ResourceTypeProvider::CONTENT_TYPE_MEDIA},
      {GURL("http://www.google.com/worker"), content::RESOURCE_TYPE_WORKER,
       ResourceTypeProvider::CONTENT_TYPE_UNKNOWN},
      {GURL("http://www.google.com/shared-worker"),
       content::RESOURCE_TYPE_SHARED_WORKER,
       ResourceTypeProvider::CONTENT_TYPE_UNKNOWN}};

  for (const auto test : tests) {
    base::HistogramTester histogram_tester;
    std::unique_ptr<net::URLRequest> request =
        CreateRequestByType(test.gurl, test.resource_type);
    request->Start();
    base::RunLoop().RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "DataReductionProxy.ResourceContentType", test.expected_content_type,
        1);

    EXPECT_EQ(test.expected_content_type,
              content_resource_type_provider()->GetContentType(request->url()));

    // Querying for the content type of |request->url()| again should still
    // give the correct result.
    EXPECT_EQ(test.expected_content_type,
              content_resource_type_provider()->GetContentType(request->url()));
  }
}

}  // namespace data_reduction_proxy