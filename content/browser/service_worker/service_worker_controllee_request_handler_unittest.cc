// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_controllee_request_handler.h"

#include <utility>
#include <vector>

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/resource_type.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/test_content_browser_client.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace content {
namespace service_worker_controllee_request_handler_unittest {

class ServiceWorkerControlleeRequestHandlerTest : public testing::Test {
 public:
  class ServiceWorkerRequestTestResources {
   public:
    ServiceWorkerRequestTestResources(
        ServiceWorkerControlleeRequestHandlerTest* test,
        const GURL& url,
        ResourceType type,
        network::mojom::RequestMode request_mode =
            network::mojom::RequestMode::kNoCors)
        : resource_type_(type),
          request_(test->url_request_context_.CreateRequest(
              url,
              net::DEFAULT_PRIORITY,
              &test->url_request_delegate_,
              TRAFFIC_ANNOTATION_FOR_TESTS)),
          handler_(std::make_unique<ServiceWorkerControlleeRequestHandler>(
              test->context()->AsWeakPtr(),
              test->provider_host_,
              type,
              /*skip_service_worker=*/false)) {}

    void MaybeCreateLoader() {
      network::ResourceRequest resource_request;
      resource_request.url = request_->url();
      resource_request.resource_type = static_cast<int>(resource_type_);
      resource_request.headers = request()->extra_request_headers();
      handler_->MaybeCreateLoader(resource_request, nullptr, nullptr,
                                  base::DoNothing(), base::DoNothing());
    }

    ServiceWorkerNavigationLoader* loader() { return handler_->loader(); }

    void SetHandler(
        std::unique_ptr<ServiceWorkerControlleeRequestHandler> handler) {
      handler_ = std::move(handler);
    }

    void ResetHandler() { handler_.reset(nullptr); }

    net::URLRequest* request() const { return request_.get(); }

   private:
    const ResourceType resource_type_;
    std::unique_ptr<net::URLRequest> request_;
    std::unique_ptr<ServiceWorkerControlleeRequestHandler> handler_;
  };

  ServiceWorkerControlleeRequestHandlerTest()
      : task_environment_(BrowserTaskEnvironment::IO_MAINLOOP) {}

  void SetUp() override {
    SetUpWithHelper(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void SetUpWithHelper(EmbeddedWorkerTestHelper* helper) {
    helper_.reset(helper);

    // A new unstored registration/version.
    scope_ = GURL("https://host/scope/");
    script_url_ = GURL("https://host/script.js");
    blink::mojom::ServiceWorkerRegistrationOptions options;
    options.scope = scope_;
    registration_ =
        new ServiceWorkerRegistration(options, 1L, context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(registration_.get(), script_url_,
                                        blink::mojom::ScriptType::kClassic, 1L,
                                        context()->AsWeakPtr());
    context()->storage()->LazyInitializeForTest();

    std::vector<ServiceWorkerDatabase::ResourceRecord> records;
    records.push_back(WriteToDiskCacheSync(
        context()->storage(), version_->script_url(),
        context()->storage()->NewResourceId(), {} /* headers */, "I'm a body",
        "I'm a meta data"));
    version_->script_cache_map()->SetResources(records);
    version_->SetMainScriptHttpResponseInfo(
        EmbeddedWorkerTestHelper::CreateHttpResponseInfo());

    // An empty host.
    remote_endpoints_.emplace_back();
    provider_host_ = CreateProviderHostForWindow(
        helper_->mock_render_process_id(), true /* is_parent_frame_secure */,
        helper_->context()->AsWeakPtr(), &remote_endpoints_.back());
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }

  void SetProviderHostIsSecure(ServiceWorkerProviderHost* host,
                               bool is_secure) {
    host->is_parent_frame_secure_ = is_secure;
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  net::URLRequestContext url_request_context_;
  net::TestDelegate url_request_delegate_;
  GURL scope_;
  GURL script_url_;
  std::vector<ServiceWorkerRemoteProviderEndpoint> remote_endpoints_;
};

class ServiceWorkerTestContentBrowserClient : public TestContentBrowserClient {
 public:
  ServiceWorkerTestContentBrowserClient() {}
  bool AllowServiceWorkerOnIO(
      const GURL& scope,
      const GURL& first_party,
      const GURL& script_url,
      content::ResourceContext* context,
      base::RepeatingCallback<WebContents*()> wc_getter) override {
    return false;
  }

  bool AllowServiceWorkerOnUI(
      const GURL& scope,
      const GURL& first_party,
      const GURL& script_url,
      content::BrowserContext* context,
      base::RepeatingCallback<WebContents*()> wc_getter) override {
    return false;
  }
};

TEST_F(ServiceWorkerControlleeRequestHandlerTest, Basic) {
  base::HistogramTester histogram_tester;

  // Prepare a valid version and registration.
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  {
    base::RunLoop loop;
    context()->storage()->StoreRegistration(
        registration_.get(), version_.get(),
        base::BindOnce(
            [](base::OnceClosure closure,
               blink::ServiceWorkerStatusCode status) {
              ASSERT_EQ(blink::ServiceWorkerStatusCode::kOk, status);
              std::move(closure).Run();
            },
            loop.QuitClosure()));
    loop.Run();
  }

  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LookupRegistration.MainResource.Time.Exists", 0);

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(test_resources.loader());
  EXPECT_TRUE(version_->HasControllee());
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LookupRegistration.MainResource.Time.Exists", 1);

  test_resources.ResetHandler();
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, DoesNotExist) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LookupRegistration.MainResource.Time.DoesNotExist", 0);

  // No version and registration exists in the scope.

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LookupRegistration.MainResource.Time.DoesNotExist", 1);

  test_resources.ResetHandler();
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, Error) {
  base::HistogramTester histogram_tester;

  // Disabling the storage makes looking up the registration return an error.
  context()->storage()->Disable();

  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LookupRegistration.MainResource.Time.Error", 0);

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  histogram_tester.ExpectTotalCount(
      "ServiceWorker.LookupRegistration.MainResource.Time.Error", 1);

  test_resources.ResetHandler();
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, DisallowServiceWorker) {
  ServiceWorkerTestContentBrowserClient test_browser_client;
  ContentBrowserClient* old_browser_client =
      SetBrowserClientForTesting(&test_browser_client);

  // Store an activated worker.
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  context()->storage()->StoreRegistration(registration_.get(), version_.get(),
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  base::RunLoop().RunUntilIdle();

  // Verify we did not use the worker.
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());

  SetBrowserClientForTesting(old_browser_client);
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, InsecureContext) {
  // Store an activated worker.
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  context()->storage()->StoreRegistration(registration_.get(), version_.get(),
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();

  SetProviderHostIsSecure(provider_host_.get(), false);

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());
  base::RunLoop().RunUntilIdle();

  // Verify we did not use the worker.
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, ActivateWaitingVersion) {
  // Store a registration that is installed but not activated yet.
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::INSTALLED);
  registration_->SetWaitingVersion(version_);
  context()->storage()->StoreRegistration(registration_.get(), version_.get(),
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());

  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, version_->status());
  EXPECT_TRUE(test_resources.loader());
  EXPECT_TRUE(version_->HasControllee());

  test_resources.ResetHandler();
}

// Test that an installing registration is associated with a provider host.
TEST_F(ServiceWorkerControlleeRequestHandlerTest, InstallingRegistration) {
  // Create an installing registration.
  version_->SetStatus(ServiceWorkerVersion::INSTALLING);
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  registration_->SetInstallingVersion(version_);
  context()->storage()->NotifyInstallingRegistration(registration_.get());

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  test_resources.MaybeCreateLoader();

  base::RunLoop().RunUntilIdle();

  // The handler should have fallen back to network and destroyed the job. The
  // provider host should not be controlled. However it should add the
  // registration as a matching registration so it can be used for .ready and
  // claim().
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());
  EXPECT_FALSE(provider_host_->controller());
  EXPECT_EQ(registration_.get(), provider_host_->MatchRegistration());
}

// Test to not regress crbug/414118.
TEST_F(ServiceWorkerControlleeRequestHandlerTest, DeletedProviderHost) {
  // Store a registration so the call to FindRegistrationForDocument will read
  // from the database.
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  context()->storage()->StoreRegistration(registration_.get(), version_.get(),
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();
  version_ = nullptr;
  registration_ = nullptr;

  // Conduct a main resource load.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  // Shouldn't crash if the ProviderHost is deleted prior to completion of
  // the database lookup.
  context()->RemoveProviderHost(provider_host_->provider_id());
  EXPECT_FALSE(provider_host_.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_resources.loader());
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, SkipServiceWorker) {
  // Store an activated worker.
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  base::RunLoop loop;
  context()->storage()->StoreRegistration(
      registration_.get(), version_.get(),
      base::BindLambdaForTesting(
          [&loop](blink::ServiceWorkerStatusCode status) { loop.Quit(); }));
  loop.Run();

  // Create an interceptor that skips service workers.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  test_resources.SetHandler(
      std::make_unique<ServiceWorkerControlleeRequestHandler>(
          context()->AsWeakPtr(), provider_host_, ResourceType::kMainFrame,
          /*skip_service_worker=*/true));

  // Conduct a main resource load.
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  base::RunLoop().RunUntilIdle();

  // Verify we did not use the worker.
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());

  // The host should still have the correct URL.
  EXPECT_EQ(GURL("https://host/scope/doc"), provider_host_->url());
}

// Tests interception after the context core has been destroyed and the provider
// host is transferred to a new context.
// TODO(crbug.com/877356): Remove this test when transferring contexts is
// removed.
TEST_F(ServiceWorkerControlleeRequestHandlerTest, NullContext) {
  // Store an activated worker.
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  base::RunLoop loop;
  context()->storage()->StoreRegistration(
      registration_.get(), version_.get(),
      base::BindLambdaForTesting(
          [&loop](blink::ServiceWorkerStatusCode status) { loop.Quit(); }));
  loop.Run();

  // Create an interceptor.
  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  test_resources.SetHandler(
      std::make_unique<ServiceWorkerControlleeRequestHandler>(
          context()->AsWeakPtr(), provider_host_, ResourceType::kMainFrame,
          /*skip_service_worker=*/false));

  // Destroy the context and make a new one.
  helper_->context_wrapper()->DeleteAndStartOver();
  base::RunLoop().RunUntilIdle();

  // Conduct a main resource load. The loader won't be created because the
  // interceptor's context is now null.
  test_resources.MaybeCreateLoader();
  EXPECT_FALSE(test_resources.loader());

  base::RunLoop().RunUntilIdle();

  // Verify we did not use the worker.
  EXPECT_FALSE(test_resources.loader());
  EXPECT_FALSE(version_->HasControllee());

  // The host should still have the correct URL.
  EXPECT_EQ(GURL("https://host/scope/doc"), provider_host_->url());
}

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
TEST_F(ServiceWorkerControlleeRequestHandlerTest, FallbackWithOfflineHeader) {
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  context()->storage()->StoreRegistration(registration_.get(), version_.get(),
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();
  version_ = NULL;
  registration_ = NULL;

  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  // Sets an offline header to indicate force loading offline page.
  test_resources.request()->SetExtraRequestHeaderByName(
      "X-Chrome-offline", "reason=download", true);
  test_resources.MaybeCreateLoader();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(test_resources.loader());
}

TEST_F(ServiceWorkerControlleeRequestHandlerTest, FallbackWithNoOfflineHeader) {
  version_->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  version_->SetStatus(ServiceWorkerVersion::ACTIVATED);
  registration_->SetActiveVersion(version_);
  context()->storage()->StoreRegistration(registration_.get(), version_.get(),
                                          base::DoNothing());
  base::RunLoop().RunUntilIdle();
  version_ = NULL;
  registration_ = NULL;

  ServiceWorkerRequestTestResources test_resources(
      this, GURL("https://host/scope/doc"), ResourceType::kMainFrame);
  // Empty offline header value should not cause fallback.
  test_resources.request()->SetExtraRequestHeaderByName("X-Chrome-offline", "",
                                                        true);
  test_resources.MaybeCreateLoader();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(test_resources.loader());
}
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGE)

}  // namespace service_worker_controllee_request_handler_unittest
}  // namespace content
