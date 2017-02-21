// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/url_schemes.h"
#include "content/public/common/origin_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_content_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

const char kServiceWorkerScheme[] = "i-can-use-service-worker";

class ServiceWorkerTestContentClient : public TestContentClient {
 public:
  void AddAdditionalSchemes(Schemes* schemes) override {
    schemes->service_worker_schemes.push_back(kServiceWorkerScheme);
  }
};

class ServiceWorkerProviderHostTest : public testing::Test {
 protected:
  ServiceWorkerProviderHostTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        next_provider_id_(1) {
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
        GURL("https://www.example.com/"), 1L, context_->AsWeakPtr());
    registration2_ = new ServiceWorkerRegistration(
        GURL("https://www.example.com/example"), 2L, context_->AsWeakPtr());
    registration3_ = new ServiceWorkerRegistration(
        GURL("https://other.example.com/"), 3L, context_->AsWeakPtr());
  }

  void TearDown() override {
    registration1_ = 0;
    registration2_ = 0;
    helper_.reset();
    SetBrowserClientForTesting(old_content_browser_client_);
    // Reset cached security schemes so we don't affect other tests.
    ResetSchemesAndOriginsWhitelist();
  }

  bool PatternHasProcessToRun(const GURL& pattern) const {
    return context_->process_manager()->PatternHasProcessToRun(pattern);
  }

  ServiceWorkerProviderHost* CreateProviderHost(const GURL& document_url) {
    std::unique_ptr<ServiceWorkerProviderHost> host =
        CreateProviderHostForWindow(
            helper_->mock_render_process_id(), next_provider_id_++,
            true /* is_parent_frame_secure */, helper_->context()->AsWeakPtr());
    ServiceWorkerProviderHost* host_raw = host.get();
    host->SetDocumentUrl(document_url);
    context_->AddProviderHost(std::move(host));
    return host_raw;
  }

  ServiceWorkerProviderHost* CreateProviderHostWithInsecureParentFrame(
      const GURL& document_url) {
    std::unique_ptr<ServiceWorkerProviderHost> host =
        CreateProviderHostForWindow(helper_->mock_render_process_id(),
                                    next_provider_id_++,
                                    false /* is_parent_frame_secure */,
                                    helper_->context()->AsWeakPtr());
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
  int next_provider_id_;

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

}  // namespace content
