// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_messages.h"
#include "content/common/url_schemes.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/origin_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"

namespace content {

namespace {

// Sets the document URL for |host| and associates it with |registration|.
// A dumb version of
// ServiceWorkerControlleeRequestHandler::PrepareForMainResource().
void SimulateServiceWorkerControlleeRequestHandler(
    ServiceWorkerProviderHost* host,
    ServiceWorkerRegistration* registration) {
  host->SetDocumentUrl(GURL("https://www.example.com/page"));
  host->AssociateRegistration(registration,
                              false /* notify_controllerchange */);
}

const char kServiceWorkerScheme[] = "i-can-use-service-worker";

class ServiceWorkerTestContentClient : public TestContentClient {
 public:
  void AddAdditionalSchemes(Schemes* schemes) override {
    schemes->service_worker_schemes.push_back(kServiceWorkerScheme);
  }
};

}  // namespace

class ServiceWorkerProviderHostTest : public testing::Test {
 protected:
  ServiceWorkerProviderHostTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        next_renderer_provided_id_(1) {
    SetContentClient(&test_content_client_);
  }
  ~ServiceWorkerProviderHostTest() override {}

  void SetUp() override {
    old_content_browser_client_ =
        SetBrowserClientForTesting(&test_content_browser_client_);
    ResetSchemesAndOriginsWhitelist();

    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
    context_ = helper_->context();
    script_url_ = GURL("https://www.example.com/service_worker.js");
    registration1_ = new ServiceWorkerRegistration(
        blink::mojom::ServiceWorkerRegistrationOptions(
            GURL("https://www.example.com/")),
        1L, context_->AsWeakPtr());
    registration2_ = new ServiceWorkerRegistration(
        blink::mojom::ServiceWorkerRegistrationOptions(
            GURL("https://www.example.com/example")),
        2L, context_->AsWeakPtr());
    registration3_ = new ServiceWorkerRegistration(
        blink::mojom::ServiceWorkerRegistrationOptions(
            GURL("https://other.example.com/")),
        3L, context_->AsWeakPtr());
  }

  void TearDown() override {
    registration1_ = nullptr;
    registration2_ = nullptr;
    registration3_ = nullptr;
    helper_.reset();
    SetBrowserClientForTesting(old_content_browser_client_);
    // Reset cached security schemes so we don't affect other tests.
    ResetSchemesAndOriginsWhitelist();
  }

  bool PatternHasProcessToRun(const GURL& pattern) const {
    return context_->process_manager()->PatternHasProcessToRun(pattern);
  }

  ServiceWorkerProviderHost* CreateProviderHost(const GURL& document_url) {
    remote_endpoints_.emplace_back();
    std::unique_ptr<ServiceWorkerProviderHost> host;
    if (IsBrowserSideNavigationEnabled()) {
      host = ServiceWorkerProviderHost::PreCreateNavigationHost(
          helper_->context()->AsWeakPtr(), true,
          base::Callback<WebContents*(void)>());
      ServiceWorkerProviderHostInfo info(host->provider_id(), 1 /* route_id */,
                                         SERVICE_WORKER_PROVIDER_FOR_WINDOW,
                                         true /* is_parent_frame_secure */);
      remote_endpoints_.back().BindWithProviderHostInfo(&info);
      host->CompleteNavigationInitialized(helper_->mock_render_process_id(),
                                          std::move(info), nullptr);
    } else {
      host = CreateProviderHostForWindow(
          helper_->mock_render_process_id(), next_renderer_provided_id_++,
          true /* is_parent_frame_secure */, helper_->context()->AsWeakPtr(),
          &remote_endpoints_.back());
    }

    ServiceWorkerProviderHost* host_raw = host.get();
    host->SetDocumentUrl(document_url);
    context_->AddProviderHost(std::move(host));
    return host_raw;
  }

  ServiceWorkerProviderHost* CreateProviderHostWithInsecureParentFrame(
      const GURL& document_url) {
    remote_endpoints_.emplace_back();
    std::unique_ptr<ServiceWorkerProviderHost> host =
        CreateProviderHostForWindow(
            helper_->mock_render_process_id(), next_renderer_provided_id_++,
            false /* is_parent_frame_secure */, helper_->context()->AsWeakPtr(),
            &remote_endpoints_.back());
    ServiceWorkerProviderHost* host_raw = host.get();
    host->SetDocumentUrl(document_url);
    context_->AddProviderHost(std::move(host));
    return host_raw;
  }

  TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  ServiceWorkerContextCore* context_;
  scoped_refptr<ServiceWorkerRegistration> registration1_;
  scoped_refptr<ServiceWorkerRegistration> registration2_;
  scoped_refptr<ServiceWorkerRegistration> registration3_;
  GURL script_url_;
  ServiceWorkerTestContentClient test_content_client_;
  TestContentBrowserClient test_content_browser_client_;
  ContentBrowserClient* old_content_browser_client_;
  int next_renderer_provided_id_;
  std::vector<ServiceWorkerRemoteProviderEndpoint> remote_endpoints_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHostTest);
};

TEST_F(ServiceWorkerProviderHostTest, PotentialRegistration_ProcessStatus) {
  ServiceWorkerProviderHost* provider_host1 =
      CreateProviderHost(GURL("https://www.example.com/example1.html"));
  ServiceWorkerProviderHost* provider_host2 =
      CreateProviderHost(GURL("https://www.example.com/example2.html"));

  // Matching registrations have already been set by SetDocumentUrl.
  ASSERT_TRUE(PatternHasProcessToRun(registration1_->pattern()));

  // Different matching registrations have already been added.
  ASSERT_TRUE(PatternHasProcessToRun(registration2_->pattern()));

  // Adding the same registration twice has no effect.
  provider_host1->AddMatchingRegistration(registration1_.get());
  ASSERT_TRUE(PatternHasProcessToRun(registration1_->pattern()));

  // Removing a matching registration will decrease the process refs for its
  // pattern.
  provider_host1->RemoveMatchingRegistration(registration1_.get());
  ASSERT_TRUE(PatternHasProcessToRun(registration1_->pattern()));
  provider_host2->RemoveMatchingRegistration(registration1_.get());
  ASSERT_FALSE(PatternHasProcessToRun(registration1_->pattern()));

  // Matching registration will be removed when moving out of scope
  ASSERT_TRUE(PatternHasProcessToRun(registration2_->pattern()));   // host1,2
  ASSERT_FALSE(PatternHasProcessToRun(registration3_->pattern()));  // no host
  provider_host1->SetDocumentUrl(GURL("https://other.example.com/"));
  ASSERT_TRUE(PatternHasProcessToRun(registration2_->pattern()));  // host2
  ASSERT_TRUE(PatternHasProcessToRun(registration3_->pattern()));  // host1
  provider_host2->SetDocumentUrl(GURL("https://other.example.com/"));
  ASSERT_FALSE(PatternHasProcessToRun(registration2_->pattern()));  // no host
  ASSERT_TRUE(PatternHasProcessToRun(registration3_->pattern()));   // host1,2
}

TEST_F(ServiceWorkerProviderHostTest, AssociatedRegistration_ProcessStatus) {
  ServiceWorkerProviderHost* provider_host1 =
      CreateProviderHost(GURL("https://www.example.com/example1.html"));

  // Associating the registration will also increase the process refs for
  // the registration's pattern.
  provider_host1->AssociateRegistration(registration1_.get(),
                                        false /* notify_controllerchange */);
  ASSERT_TRUE(PatternHasProcessToRun(registration1_->pattern()));

  // Disassociating the registration shouldn't affect the process refs for
  // the registration's pattern.
  provider_host1->DisassociateRegistration();
  ASSERT_TRUE(PatternHasProcessToRun(registration1_->pattern()));
}

TEST_F(ServiceWorkerProviderHostTest, MatchRegistration) {
  ServiceWorkerProviderHost* provider_host1 =
      CreateProviderHost(GURL("https://www.example.com/example1.html"));

  // Match registration should return the longest matching one.
  ASSERT_EQ(registration2_, provider_host1->MatchRegistration());
  provider_host1->RemoveMatchingRegistration(registration2_.get());
  ASSERT_EQ(registration1_, provider_host1->MatchRegistration());

  // Should return nullptr after removing all matching registrations.
  provider_host1->RemoveMatchingRegistration(registration1_.get());
  ASSERT_EQ(nullptr, provider_host1->MatchRegistration());

  // SetDocumentUrl sets all of matching registrations
  provider_host1->SetDocumentUrl(GURL("https://www.example.com/example1"));
  ASSERT_EQ(registration2_, provider_host1->MatchRegistration());
  provider_host1->RemoveMatchingRegistration(registration2_.get());
  ASSERT_EQ(registration1_, provider_host1->MatchRegistration());

  // SetDocumentUrl with another origin also updates matching registrations
  provider_host1->SetDocumentUrl(GURL("https://other.example.com/example"));
  ASSERT_EQ(registration3_, provider_host1->MatchRegistration());
  provider_host1->RemoveMatchingRegistration(registration3_.get());
  ASSERT_EQ(nullptr, provider_host1->MatchRegistration());
}

TEST_F(ServiceWorkerProviderHostTest, ContextSecurity) {
  ServiceWorkerProviderHost* provider_host_secure_parent =
      CreateProviderHost(GURL("https://www.example.com/example1.html"));
  ServiceWorkerProviderHost* provider_host_insecure_parent =
      CreateProviderHostWithInsecureParentFrame(
          GURL("https://www.example.com/example1.html"));

  // Insecure document URL.
  provider_host_secure_parent->SetDocumentUrl(GURL("http://host"));
  EXPECT_FALSE(provider_host_secure_parent->IsContextSecureForServiceWorker());

  // Insecure parent frame.
  provider_host_insecure_parent->SetDocumentUrl(GURL("https://host"));
  EXPECT_FALSE(
      provider_host_insecure_parent->IsContextSecureForServiceWorker());

  // Secure URL and parent frame.
  provider_host_secure_parent->SetDocumentUrl(GURL("https://host"));
  EXPECT_TRUE(provider_host_secure_parent->IsContextSecureForServiceWorker());

  // Exceptional service worker scheme.
  GURL url(std::string(kServiceWorkerScheme) + "://host");
  EXPECT_TRUE(url.is_valid());
  EXPECT_FALSE(IsOriginSecure(url));
  EXPECT_TRUE(OriginCanAccessServiceWorkers(url));
  provider_host_secure_parent->SetDocumentUrl(url);
  EXPECT_TRUE(provider_host_secure_parent->IsContextSecureForServiceWorker());

  // Exceptional service worker scheme with insecure parent frame.
  provider_host_insecure_parent->SetDocumentUrl(url);
  EXPECT_FALSE(
      provider_host_insecure_parent->IsContextSecureForServiceWorker());
}

class MockServiceWorkerRegistration : public ServiceWorkerRegistration {
 public:
  MockServiceWorkerRegistration(
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      int64_t registration_id,
      base::WeakPtr<ServiceWorkerContextCore> context)
      : ServiceWorkerRegistration(options, registration_id, context) {}

  void AddListener(ServiceWorkerRegistration::Listener* listener) override {
    listeners_.insert(listener);
  }

  void RemoveListener(ServiceWorkerRegistration::Listener* listener) override {
    listeners_.erase(listener);
  }

  const std::set<ServiceWorkerRegistration::Listener*>& listeners() {
    return listeners_;
  }

 protected:
  ~MockServiceWorkerRegistration() override{};

 private:
  std::set<ServiceWorkerRegistration::Listener*> listeners_;
};

TEST_F(ServiceWorkerProviderHostTest, CrossSiteTransfer) {
  if (IsBrowserSideNavigationEnabled())
    return;

  // Create a mock registration before creating the provider host which is in
  // the scope.
  blink::mojom::ServiceWorkerRegistrationOptions options(
      GURL("https://cross.example.com/"));
  scoped_refptr<MockServiceWorkerRegistration> registration =
      new MockServiceWorkerRegistration(options, 4L,
                                        helper_->context()->AsWeakPtr());

  ServiceWorkerProviderHost* provider_host =
      CreateProviderHost(GURL("https://cross.example.com/example.html"));
  const int process_id = provider_host->process_id();
  const int provider_id = provider_host->provider_id();
  const int frame_id = provider_host->frame_id();
  const ServiceWorkerProviderType type = provider_host->provider_type();
  const bool is_parent_frame_secure = provider_host->is_parent_frame_secure();
  const ServiceWorkerDispatcherHost* dispatcher_host =
      provider_host->dispatcher_host();

  EXPECT_EQ(1u, registration->listeners().count(provider_host));

  std::unique_ptr<ServiceWorkerProviderHost> provisional_host =
      provider_host->PrepareForCrossSiteTransfer();

  EXPECT_EQ(process_id, provisional_host->process_id());
  EXPECT_EQ(provider_id, provisional_host->provider_id());
  EXPECT_EQ(frame_id, provisional_host->frame_id());
  EXPECT_EQ(type, provisional_host->provider_type());
  EXPECT_EQ(is_parent_frame_secure, provisional_host->is_parent_frame_secure());
  EXPECT_EQ(dispatcher_host, provisional_host->dispatcher_host());

  EXPECT_EQ(ChildProcessHost::kInvalidUniqueID, provider_host->process_id());
  EXPECT_EQ(kInvalidServiceWorkerProviderId, provider_host->provider_id());
  EXPECT_EQ(MSG_ROUTING_NONE, provider_host->frame_id());
  EXPECT_EQ(SERVICE_WORKER_PROVIDER_UNKNOWN, provider_host->provider_type());
  EXPECT_FALSE(provider_host->is_parent_frame_secure());
  EXPECT_EQ(nullptr, provider_host->dispatcher_host());

  EXPECT_EQ(0u, registration->listeners().size());

  provider_host->CompleteCrossSiteTransfer(provisional_host.get());

  EXPECT_EQ(process_id, provider_host->process_id());
  EXPECT_EQ(provider_id, provider_host->provider_id());
  EXPECT_EQ(frame_id, provider_host->frame_id());
  EXPECT_EQ(type, provider_host->provider_type());
  EXPECT_EQ(is_parent_frame_secure, provider_host->is_parent_frame_secure());
  EXPECT_EQ(dispatcher_host, provider_host->dispatcher_host());

  EXPECT_EQ(kInvalidServiceWorkerProviderId, provisional_host->provider_id());
  EXPECT_EQ(MSG_ROUTING_NONE, provisional_host->frame_id());
  EXPECT_EQ(SERVICE_WORKER_PROVIDER_UNKNOWN, provisional_host->provider_type());
  EXPECT_FALSE(provisional_host->is_parent_frame_secure());
  EXPECT_EQ(nullptr, provisional_host->dispatcher_host());

  EXPECT_EQ(1u, registration->listeners().count(provider_host));
}

TEST_F(ServiceWorkerProviderHostTest, RemoveProvider) {
  // Create a provider host connected with the renderer process.
  ServiceWorkerProviderHost* provider_host =
      CreateProviderHost(GURL("https://www.example.com/example1.html"));
  int process_id = provider_host->process_id();
  int provider_id = provider_host->provider_id();
  EXPECT_TRUE(context_->GetProviderHost(process_id, provider_id));

  // Disconnect the mojo pipe from the renderer side.
  ASSERT_TRUE(remote_endpoints_.back().host_ptr()->is_bound());
  remote_endpoints_.back().host_ptr()->reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(context_->GetProviderHost(process_id, provider_id));
}

TEST_F(ServiceWorkerProviderHostTest, Controller) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableBrowserSideNavigation);
  // Create a host.
  std::unique_ptr<ServiceWorkerProviderHost> host =
      ServiceWorkerProviderHost::PreCreateNavigationHost(
          helper_->context()->AsWeakPtr(), true /* are_ancestors_secure */,
          base::Callback<WebContents*(void)>());
  ServiceWorkerProviderHostInfo info(host->provider_id(), 1 /* route_id */,
                                     SERVICE_WORKER_PROVIDER_FOR_WINDOW,
                                     true /* is_parent_frame_secure */);
  remote_endpoints_.emplace_back();
  remote_endpoints_.back().BindWithProviderHostInfo(&info);

  // Create an active version and then start the navigation.
  scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      1 /* version_id */, helper_->context()->AsWeakPtr());
  registration1_->SetActiveVersion(version);
  SimulateServiceWorkerControlleeRequestHandler(host.get(),
                                                registration1_.get());

  // Finish the navigation.
  host->CompleteNavigationInitialized(
      helper_->mock_render_process_id(), std::move(info),
      helper_->GetDispatcherHostForProcess(helper_->mock_render_process_id())
          ->AsWeakPtr());

  // The page should be controlled since there was an active version at the
  // time navigation started. The SetController IPC should have been sent.
  EXPECT_TRUE(host->active_version());
  EXPECT_EQ(host->active_version(), host->controller());
  EXPECT_TRUE(helper_->ipc_sink()->GetUniqueMessageMatching(
      ServiceWorkerMsg_SetControllerServiceWorker::ID));
}

TEST_F(ServiceWorkerProviderHostTest, ActiveIsNotController) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableBrowserSideNavigation);
  // Create a host.
  std::unique_ptr<ServiceWorkerProviderHost> host =
      ServiceWorkerProviderHost::PreCreateNavigationHost(
          helper_->context()->AsWeakPtr(), true /* are_ancestors_secure */,
          base::Callback<WebContents*(void)>());
  ServiceWorkerProviderHostInfo info(host->provider_id(), 1 /* route_id */,
                                     SERVICE_WORKER_PROVIDER_FOR_WINDOW,
                                     true /* is_parent_frame_secure */);
  remote_endpoints_.emplace_back();
  remote_endpoints_.back().BindWithProviderHostInfo(&info);

  // Associate it with an installing registration then start the navigation.
  scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
      registration1_.get(), GURL("https://www.example.com/sw.js"),
      1 /* version_id */, helper_->context()->AsWeakPtr());
  registration1_->SetInstallingVersion(version);
  SimulateServiceWorkerControlleeRequestHandler(host.get(),
                                                registration1_.get());

  // Promote the worker to active while navigation is still happening.
  registration1_->SetActiveVersion(version);

  // Finish the navigation.
  host->CompleteNavigationInitialized(
      helper_->mock_render_process_id(), std::move(info),
      helper_->GetDispatcherHostForProcess(helper_->mock_render_process_id())
          ->AsWeakPtr());

  // The page should not be controlled since there was no active version at the
  // time navigation started. Furthermore, no SetController IPC should have been
  // sent.
  EXPECT_TRUE(host->active_version());
  EXPECT_FALSE(host->controller());
  EXPECT_EQ(nullptr, helper_->ipc_sink()->GetFirstMessageMatching(
                         ServiceWorkerMsg_SetControllerServiceWorker::ID));
}

}  // namespace content
