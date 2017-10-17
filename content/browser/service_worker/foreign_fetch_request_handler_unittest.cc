// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/foreign_fetch_request_handler.h"

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/origin_trial_policy.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/test_content_browser_client.h"
#include "net/http/http_response_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace content {

namespace {

// This is a sample public key for testing the API. The corresponding private
// key (use this to generate new samples for this test file) is:
//
//  0x83, 0x67, 0xf4, 0xcd, 0x2a, 0x1f, 0x0e, 0x04, 0x0d, 0x43, 0x13,
//  0x4c, 0x67, 0xc4, 0xf4, 0x28, 0xc9, 0x90, 0x15, 0x02, 0xe2, 0xba,
//  0xfd, 0xbb, 0xfa, 0xbc, 0x92, 0x76, 0x8a, 0x2c, 0x4b, 0xc7, 0x75,
//  0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2, 0x9a,
//  0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f, 0x64,
//  0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0
const uint8_t kTestPublicKey[] = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

int kMockProviderId = 1;

const char* kValidUrl = "https://valid.example.com/foo/bar";

// This timestamp is set to a time after the expiry timestamp of the expired
// tokens in this test, but before the expiry timestamp of the valid ones.
double kNowTimestamp = 1500000000;

}  // namespace

class ForeignFetchRequestHandlerTest : public testing::Test {
 public:
  ForeignFetchRequestHandlerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {
    SetContentClient(&test_content_client_);
    SetBrowserClientForTesting(&test_content_browser_client_);
  }
  ~ForeignFetchRequestHandlerTest() override {}

  void SetUp() override {
    const GURL kScope("https://valid.example.com/scope/");
    const GURL kResource1("https://valid.example.com/scope/sw.js");
    const int64_t kRegistrationId = 0;
    const int64_t kVersionId = 0;
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));

    // Create a registration for the worker which has foreign fetch event
    // handler.
    registration_ = new ServiceWorkerRegistration(
        blink::mojom::ServiceWorkerRegistrationOptions(kScope), kRegistrationId,
        context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(registration_.get(), kResource1,
                                        kVersionId, context()->AsWeakPtr());
    version_->set_foreign_fetch_scopes({kScope});

    // Fix the time for testing to kNowTimestamp
    std::unique_ptr<base::SimpleTestClock> clock =
        base::MakeUnique<base::SimpleTestClock>();
    clock->SetNow(base::Time::FromDoubleT(kNowTimestamp));
    version_->SetClockForTesting(std::move(clock));

    context()->storage()->LazyInitializeForTest(
        base::BindOnce(&base::DoNothing));
    base::RunLoop().RunUntilIdle();

    // Persist the registration data.
    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(WriteToDiskCacheSync(
        context()->storage(), version_->script_url(), 10, {} /* headers */,
        "I'm the body", "I'm the meta data"));
    version_->script_cache_map()->SetResources(records);
    version_->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
    registration_->SetActiveVersion(version_);
    context()->storage()->StoreRegistration(
        registration_.get(), version_.get(),
        base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  ServiceWorkerContextCore* context() const { return helper_->context(); }
  ServiceWorkerContextWrapper* context_wrapper() const {
    return helper_->context_wrapper();
  }
  ServiceWorkerProviderHost* provider_host() const {
    return provider_host_.get();
  }

  bool CheckOriginTrialToken(const ServiceWorkerVersion* const version) const {
    return ForeignFetchRequestHandler::CheckOriginTrialToken(version);
  }

  base::Optional<base::TimeDelta> timeout_for_request(
      ForeignFetchRequestHandler* handler) {
    return handler->timeout_;
  }

  ServiceWorkerVersion* version() const { return version_.get(); }

  static std::unique_ptr<net::HttpResponseInfo> CreateTestHttpResponseInfo() {
    std::unique_ptr<net::HttpResponseInfo> http_info(
        base::MakeUnique<net::HttpResponseInfo>());
    http_info->ssl_info.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    DCHECK(http_info->ssl_info.is_valid());
    http_info->ssl_info.security_bits = 0x100;
    // SSL3 TLS_DHE_RSA_WITH_AES_256_CBC_SHA
    http_info->ssl_info.connection_status = 0x300039;
    http_info->headers = base::MakeRefCounted<net::HttpResponseHeaders>("");
    return http_info;
  }

  ForeignFetchRequestHandler* InitializeHandler(const std::string& url,
                                                ResourceType resource_type,
                                                const char* initiator) {
    request_ = url_request_context_.CreateRequest(
        GURL(url), net::DEFAULT_PRIORITY, &url_request_delegate_,
        TRAFFIC_ANNOTATION_FOR_TESTS);
    if (initiator)
      request_->set_initiator(url::Origin(GURL(initiator)));
    ForeignFetchRequestHandler::InitializeHandler(
        request_.get(), context_wrapper(), &blob_storage_context_,
        helper_->mock_render_process_id(), provider_host()->provider_id(),
        ServiceWorkerMode::ALL, FETCH_REQUEST_MODE_CORS,
        FETCH_CREDENTIALS_MODE_OMIT, FetchRedirectMode::FOLLOW_MODE,
        std::string() /* integrity */, resource_type,
        REQUEST_CONTEXT_TYPE_FETCH, REQUEST_CONTEXT_FRAME_TYPE_NONE, nullptr,
        true /* initiated_in_secure_context */);

    return ForeignFetchRequestHandler::GetHandler(request_.get());
  }

  void CreateWindowTypeProviderHost() {
    remote_endpoints_.emplace_back();
    std::unique_ptr<ServiceWorkerProviderHost> host =
        CreateProviderHostForWindow(
            helper_->mock_render_process_id(), kMockProviderId,
            true /* is_parent_frame_secure */, helper_->context()->AsWeakPtr(),
            &remote_endpoints_.back());
    EXPECT_FALSE(
        context()->GetProviderHost(host->process_id(), host->provider_id()));
    host->SetDocumentUrl(GURL("https://host/scope/"));
    provider_host_ = host->AsWeakPtr();
    context()->AddProviderHost(std::move(host));
  }

  void CreateServiceWorkerTypeProviderHost() {
    // Create another worker whose requests will be intercepted by the foreign
    // fetch event handler.
    scoped_refptr<ServiceWorkerRegistration> registration =
        new ServiceWorkerRegistration(
            blink::mojom::ServiceWorkerRegistrationOptions(
                GURL("https://host/scope")),
            1L, context()->AsWeakPtr());
    scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
        registration.get(), GURL("https://host/script.js"), 1L,
        context()->AsWeakPtr());

    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(
        ServiceWorkerDatabase::ResourceRecord(10, version->script_url(), 100));
    version->script_cache_map()->SetResources(records);
    version->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    version->SetStatus(ServiceWorkerVersion::ACTIVATED);
    registration->SetActiveVersion(version);
    context()->storage()->StoreRegistration(
        registration.get(), version.get(),
        base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
    base::RunLoop().RunUntilIdle();

    remote_endpoints_.emplace_back();
    std::unique_ptr<ServiceWorkerProviderHost> host =
        CreateProviderHostForServiceWorkerContext(
            helper_->mock_render_process_id(),
            true /* is_parent_frame_secure */, version.get(),
            helper_->context()->AsWeakPtr(), &remote_endpoints_.back());
    EXPECT_FALSE(
        context()->GetProviderHost(host->process_id(), host->provider_id()));
    provider_host_ = host->AsWeakPtr();
    context()->AddProviderHost(std::move(host));
  }

 private:
  class TestContentClient : public ContentClient {
   public:
    // ContentRendererClient methods
    OriginTrialPolicy* GetOriginTrialPolicy() override {
      return &origin_trial_policy_;
    }

   private:
    class TestOriginTrialPolicy : public OriginTrialPolicy {
     public:
      base::StringPiece GetPublicKey() const override {
        return base::StringPiece(reinterpret_cast<const char*>(kTestPublicKey),
                                 arraysize(kTestPublicKey));
      }
    };

    TestOriginTrialPolicy origin_trial_policy_;
  };

  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  TestContentClient test_content_client_;
  TestContentBrowserClient test_content_browser_client_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  TestBrowserThreadBundle browser_thread_bundle_;

  net::URLRequestContext url_request_context_;
  net::TestDelegate url_request_delegate_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  storage::BlobStorageContext blob_storage_context_;
  std::unique_ptr<net::URLRequest> request_;
  std::vector<ServiceWorkerRemoteProviderEndpoint> remote_endpoints_;

  DISALLOW_COPY_AND_ASSIGN(ForeignFetchRequestHandlerTest);
};

TEST_F(ForeignFetchRequestHandlerTest, CheckOriginTrialToken_NoToken) {
  CreateWindowTypeProviderHost();
  EXPECT_TRUE(CheckOriginTrialToken(version()));
  std::unique_ptr<net::HttpResponseInfo> http_info(
      CreateTestHttpResponseInfo());
  version()->SetMainScriptHttpResponseInfo(*http_info);
  EXPECT_FALSE(CheckOriginTrialToken(version()));
}

TEST_F(ForeignFetchRequestHandlerTest, CheckOriginTrialToken_ValidToken) {
  CreateWindowTypeProviderHost();
  EXPECT_TRUE(CheckOriginTrialToken(version()));
  std::unique_ptr<net::HttpResponseInfo> http_info(
      CreateTestHttpResponseInfo());
  const std::string kOriginTrial("Origin-Trial: ");
  // Token for ForeignFetch which expires 2033-05-18.
  // generate_token.py valid.example.com ForeignFetch
  // --expire-timestamp=2000000000
  // TODO(horo): Generate this sample token during the build.
  const std::string kFeatureToken(
      "AsDmvl17hoVfq9G6OT0VEhX28Nnl0VnbGZJoG6XFzosIamNfxNJ28m40PRA3PtFv3BfOlRe1"
      "5bLrEZhtICdDnwwAAABceyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGb3JlaWduRmV0Y2giLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0"
      "=");
  http_info->headers->AddHeader(kOriginTrial + kFeatureToken);
  version()->SetMainScriptHttpResponseInfo(*http_info);
  EXPECT_TRUE(CheckOriginTrialToken(version()));
}

TEST_F(ForeignFetchRequestHandlerTest, CheckOriginTrialToken_InvalidToken) {
  CreateWindowTypeProviderHost();
  EXPECT_TRUE(CheckOriginTrialToken(version()));
  std::unique_ptr<net::HttpResponseInfo> http_info(
      CreateTestHttpResponseInfo());
  const std::string kOriginTrial("Origin-Trial: ");
  // Token for FooBar which expires 2033-05-18.
  // generate_token.py valid.example.com FooBar
  // --expire-timestamp=2000000000
  // TODO(horo): Generate this sample token during the build.
  const std::string kFeatureToken(
      "AqwtRpuoLdc6MKSFH8TbzoLFvLouL8VXTv6OJIqQvRtJBynRMbAbFwjUlcwMuj4pXUBbquBj"
      "zj/w/d1H/ZsOcQIAAABWeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGb29CYXIiLCAiZXhwaXJ5IjogMjAwMDAwMDAwMH0=");
  http_info->headers->AddHeader(kOriginTrial + kFeatureToken);
  version()->SetMainScriptHttpResponseInfo(*http_info);
  EXPECT_FALSE(CheckOriginTrialToken(version()));
}

TEST_F(ForeignFetchRequestHandlerTest, CheckOriginTrialToken_ExpiredToken) {
  CreateWindowTypeProviderHost();
  EXPECT_TRUE(CheckOriginTrialToken(version()));
  std::unique_ptr<net::HttpResponseInfo> http_info(
      CreateTestHttpResponseInfo());
  const std::string kOriginTrial("Origin-Trial: ");
  // Token for ForeignFetch which expired 2001-09-09.
  // generate_token.py valid.example.com ForeignFetch
  // --expire-timestamp=1000000000
  const std::string kFeatureToken(
      "AgBgj4Zhwzn85LJw7rzh4ZFRFqp49+9Es2SrCwZdDcoqtqQEjbvui4SKLn6GqMpr4DynGfJh"
      "tIy9dpOuK8PVTwkAAABceyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5leGFtcGxlLmNvbTo0"
      "NDMiLCAiZmVhdHVyZSI6ICJGb3JlaWduRmV0Y2giLCAiZXhwaXJ5IjogMTAwMDAwMDAwMH0"
      "=");
  http_info->headers->AddHeader(kOriginTrial + kFeatureToken);
  version()->SetMainScriptHttpResponseInfo(*http_info);
  EXPECT_FALSE(CheckOriginTrialToken(version()));
}

TEST_F(ForeignFetchRequestHandlerTest, InitializeHandler_Success) {
  CreateWindowTypeProviderHost();
  EXPECT_TRUE(InitializeHandler(kValidUrl, RESOURCE_TYPE_IMAGE,
                                nullptr /* initiator */));
}

TEST_F(ForeignFetchRequestHandlerTest, InitializeHandler_WrongResourceType) {
  CreateWindowTypeProviderHost();
  EXPECT_FALSE(InitializeHandler(kValidUrl, RESOURCE_TYPE_MAIN_FRAME,
                                 nullptr /* initiator */));
  EXPECT_FALSE(InitializeHandler(kValidUrl, RESOURCE_TYPE_SUB_FRAME,
                                 nullptr /* initiator */));
  EXPECT_FALSE(InitializeHandler(kValidUrl, RESOURCE_TYPE_WORKER,
                                 nullptr /* initiator */));
  EXPECT_FALSE(InitializeHandler(kValidUrl, RESOURCE_TYPE_SHARED_WORKER,
                                 nullptr /* initiator */));
  EXPECT_FALSE(InitializeHandler(kValidUrl, RESOURCE_TYPE_SERVICE_WORKER,
                                 nullptr /* initiator */));
}

TEST_F(ForeignFetchRequestHandlerTest, InitializeHandler_SameOriginRequest) {
  CreateWindowTypeProviderHost();
  EXPECT_FALSE(InitializeHandler(kValidUrl, RESOURCE_TYPE_IMAGE,
                                 kValidUrl /* initiator */));
}

TEST_F(ForeignFetchRequestHandlerTest, InitializeHandler_NoRegisteredHandlers) {
  CreateWindowTypeProviderHost();
  EXPECT_FALSE(InitializeHandler("https://invalid.example.com/foo",
                                 RESOURCE_TYPE_IMAGE, nullptr /* initiator */));
}

TEST_F(ForeignFetchRequestHandlerTest,
       InitializeHandler_TimeoutBehaviorForWindow) {
  CreateWindowTypeProviderHost();
  ForeignFetchRequestHandler* handler =
      InitializeHandler("https://valid.example.com/foo", RESOURCE_TYPE_IMAGE,
                        nullptr /* initiator */);
  ASSERT_TRUE(handler);

  EXPECT_EQ(base::nullopt, timeout_for_request(handler));
}

TEST_F(ForeignFetchRequestHandlerTest,
       InitializeHandler_TimeoutBehaviorForServiceWorker) {
  CreateServiceWorkerTypeProviderHost();
  ServiceWorkerVersion* version = provider_host()->running_hosted_version();
  std::unique_ptr<net::HttpResponseInfo> http_info(
      CreateTestHttpResponseInfo());
  version->SetMainScriptHttpResponseInfo(*http_info);

  // Set mock clock on version to check timeout behavior.
  base::SimpleTestTickClock* tick_clock = new base::SimpleTestTickClock();
  tick_clock->SetNowTicks(base::TimeTicks::Now());
  version->SetTickClockForTesting(base::WrapUnique(tick_clock));

  // Make sure worker has a non-zero timeout.
  version->StartWorker(ServiceWorkerMetrics::EventType::UNKNOWN,
                       base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();
  version->StartRequestWithCustomTimeout(
      ServiceWorkerMetrics::EventType::ACTIVATE,
      base::BindOnce(&ServiceWorkerUtils::NoOpStatusCallback),
      base::TimeDelta::FromSeconds(10), ServiceWorkerVersion::KILL_ON_TIMEOUT);

  // Advance clock by a couple seconds.
  tick_clock->Advance(base::TimeDelta::FromSeconds(4));
  base::TimeDelta remaining_time = version->remaining_timeout();
  EXPECT_EQ(base::TimeDelta::FromSeconds(6), remaining_time);

  // Make sure new request only gets remaining timeout.
  ForeignFetchRequestHandler* handler =
      InitializeHandler("https://valid.example.com/foo", RESOURCE_TYPE_IMAGE,
                        nullptr /* initiator */);
  ASSERT_TRUE(handler);
  ASSERT_TRUE(timeout_for_request(handler).has_value());
  EXPECT_EQ(remaining_time, timeout_for_request(handler).value());
}

}  // namespace content
