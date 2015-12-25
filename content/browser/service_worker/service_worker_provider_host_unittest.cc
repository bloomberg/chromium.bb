// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_register_job.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class ServiceWorkerProviderHostTest : public testing::Test {
 protected:
  ServiceWorkerProviderHostTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}
  ~ServiceWorkerProviderHostTest() override {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
    context_ = helper_->context();
    script_url_ = GURL("http://www.example.com/service_worker.js");
    registration1_ = new ServiceWorkerRegistration(
        GURL("http://www.example.com/"), 1L, context_->AsWeakPtr());
    registration2_ = new ServiceWorkerRegistration(
        GURL("http://www.example.com/example"), 2L, context_->AsWeakPtr());

    // Prepare provider hosts (for the same process).
    scoped_ptr<ServiceWorkerProviderHost> host1(new ServiceWorkerProviderHost(
        helper_->mock_render_process_id(), MSG_ROUTING_NONE,
        1 /* provider_id */, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
        context_->AsWeakPtr(), NULL));
    host1->SetDocumentUrl(GURL("http://www.example.com/example1.html"));
    scoped_ptr<ServiceWorkerProviderHost> host2(new ServiceWorkerProviderHost(
        helper_->mock_render_process_id(), MSG_ROUTING_NONE,
        2 /* provider_id */, SERVICE_WORKER_PROVIDER_FOR_WINDOW,
        context_->AsWeakPtr(), NULL));
    host2->SetDocumentUrl(GURL("http://www.example.com/example2.html"));
    provider_host1_ = host1->AsWeakPtr();
    provider_host2_ = host2->AsWeakPtr();
    context_->AddProviderHost(make_scoped_ptr(host1.release()));
    context_->AddProviderHost(make_scoped_ptr(host2.release()));
  }

  void TearDown() override {
    registration1_ = 0;
    registration2_ = 0;
    helper_.reset();
  }

  bool PatternHasProcessToRun(const GURL& pattern) const {
    return context_->process_manager()->PatternHasProcessToRun(pattern);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  ServiceWorkerContextCore* context_;
  scoped_refptr<ServiceWorkerRegistration> registration1_;
  scoped_refptr<ServiceWorkerRegistration> registration2_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host1_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host2_;
  GURL script_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHostTest);
};

TEST_F(ServiceWorkerProviderHostTest, PotentialRegistration_ProcessStatus) {
  provider_host1_->AddMatchingRegistration(registration1_.get());
  ASSERT_TRUE(PatternHasProcessToRun(registration1_->pattern()));

  // Adding the same registration twice has no effect.
  provider_host1_->AddMatchingRegistration(registration1_.get());
  ASSERT_TRUE(PatternHasProcessToRun(registration1_->pattern()));

  // Different matching registrations can be added.
  provider_host1_->AddMatchingRegistration(registration2_.get());
  ASSERT_TRUE(PatternHasProcessToRun(registration2_->pattern()));

  // Removing a matching registration will decrease the process refs for its
  // pattern.
  provider_host1_->RemoveMatchingRegistration(registration1_.get());
  ASSERT_FALSE(PatternHasProcessToRun(registration1_->pattern()));

  // Multiple provider hosts could add the same matching registration.
  // The process refs will become 0 after all provider hosts removed them.
  provider_host2_->AddMatchingRegistration(registration2_.get());
  provider_host1_->RemoveMatchingRegistration(registration2_.get());
  ASSERT_TRUE(PatternHasProcessToRun(registration2_->pattern()));
  provider_host2_->RemoveMatchingRegistration(registration2_.get());
  ASSERT_FALSE(PatternHasProcessToRun(registration2_->pattern()));
}

TEST_F(ServiceWorkerProviderHostTest, AssociatedRegistration_ProcessStatus) {
  // Associating the registration will also increase the process refs for
  // the registration's pattern.
  provider_host1_->AssociateRegistration(registration1_.get(),
                                         false /* notify_controllerchange */);
  ASSERT_TRUE(PatternHasProcessToRun(registration1_->pattern()));

  // Disassociating the registration shouldn't affect the process refs for
  // the registration's pattern.
  provider_host1_->DisassociateRegistration();
  ASSERT_TRUE(PatternHasProcessToRun(registration1_->pattern()));
}

TEST_F(ServiceWorkerProviderHostTest, MatchRegistration) {
  provider_host1_->AddMatchingRegistration(registration1_.get());
  provider_host1_->AddMatchingRegistration(registration2_.get());

  // Match registration should return the longest matching one.
  ASSERT_EQ(provider_host1_->MatchRegistration(), registration2_);
  provider_host1_->RemoveMatchingRegistration(registration2_.get());
  ASSERT_EQ(provider_host1_->MatchRegistration(), registration1_);

  // Should return nullptr after removing all matching registrations.
  provider_host1_->RemoveMatchingRegistration(registration1_.get());
  ASSERT_EQ(provider_host1_->MatchRegistration(), nullptr);
}

}  // namespace content
