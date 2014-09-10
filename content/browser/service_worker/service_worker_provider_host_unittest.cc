// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
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

static const int kRenderProcessId = 33;  // Dummy process ID for testing.

class ServiceWorkerProviderHostTest : public testing::Test {
 protected:
  ServiceWorkerProviderHostTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}
  virtual ~ServiceWorkerProviderHostTest() {}

  virtual void SetUp() OVERRIDE {
    helper_.reset(new EmbeddedWorkerTestHelper(kRenderProcessId));
    context_ = helper_->context();
    pattern_ = GURL("http://www.example.com/");
    script_url_ = GURL("http://www.example.com/service_worker.js");
    registration_ = new ServiceWorkerRegistration(
        pattern_, 1L, context_->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(), script_url_, 1L, context_->AsWeakPtr());

    // Prepare provider hosts (for the same process).
    scoped_ptr<ServiceWorkerProviderHost> host1(new ServiceWorkerProviderHost(
        kRenderProcessId, 1 /* provider_id */,
        context_->AsWeakPtr(), NULL));
    scoped_ptr<ServiceWorkerProviderHost> host2(new ServiceWorkerProviderHost(
        kRenderProcessId, 2 /* provider_id */,
        context_->AsWeakPtr(), NULL));
    provider_host1_ = host1->AsWeakPtr();
    provider_host2_ = host2->AsWeakPtr();
    context_->AddProviderHost(make_scoped_ptr(host1.release()));
    context_->AddProviderHost(make_scoped_ptr(host2.release()));
  }

  virtual void TearDown() OVERRIDE {
    version_ = 0;
    registration_ = 0;
    helper_.reset();
  }

  bool HasProcessToRun() const {
    return context_->process_manager()->PatternHasProcessToRun(pattern_);
  }

  void VerifyVersionAttributes(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ServiceWorkerVersion* installing,
      ServiceWorkerVersion* waiting,
      ServiceWorkerVersion* active) {
    EXPECT_EQ(installing, provider_host->installing_version_.get());
    EXPECT_EQ(waiting, provider_host->waiting_version_.get());
    EXPECT_EQ(active, provider_host->active_version_.get());
    EXPECT_FALSE(provider_host->controlling_version_.get());
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  ServiceWorkerContextCore* context_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host1_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host2_;
  GURL pattern_;
  GURL script_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHostTest);
};

TEST_F(ServiceWorkerProviderHostTest, SetActiveVersion_ProcessStatus) {
  provider_host1_->AssociateRegistration(registration_.get());
  ASSERT_TRUE(HasProcessToRun());

  // Associating version_ to a provider_host's active version will internally
  // add the provider_host's process ref to the version.
  registration_->SetActiveVersion(version_.get());
  ASSERT_TRUE(HasProcessToRun());

  // Re-associating the same version and provider_host should just work too.
  registration_->SetActiveVersion(version_.get());
  ASSERT_TRUE(HasProcessToRun());

  // Resetting the provider_host's active version should remove process refs
  // from the version.
  provider_host1_->UnassociateRegistration();
  ASSERT_FALSE(HasProcessToRun());
}

TEST_F(ServiceWorkerProviderHostTest,
       SetActiveVersion_MultipleHostsForSameProcess) {
  provider_host1_->AssociateRegistration(registration_.get());
  provider_host2_->AssociateRegistration(registration_.get());
  ASSERT_TRUE(HasProcessToRun());

  // Associating version_ to two providers as active version.
  registration_->SetActiveVersion(version_.get());
  ASSERT_TRUE(HasProcessToRun());

  // Disassociating one provider_host shouldn't remove all process refs
  // from the version yet.
  provider_host1_->UnassociateRegistration();
  ASSERT_TRUE(HasProcessToRun());

  // Disassociating the other provider_host will remove all process refs.
  provider_host2_->UnassociateRegistration();
  ASSERT_FALSE(HasProcessToRun());
}

TEST_F(ServiceWorkerProviderHostTest, SetWaitingVersion_ProcessStatus) {
  provider_host1_->AssociateRegistration(registration_.get());
  ASSERT_TRUE(HasProcessToRun());

  // Associating version_ to a provider_host's waiting version will internally
  // add the provider_host's process ref to the version.
  registration_->SetWaitingVersion(version_.get());
  ASSERT_TRUE(HasProcessToRun());

  // Re-associating the same version and provider_host should just work too.
  registration_->SetWaitingVersion(version_.get());
  ASSERT_TRUE(HasProcessToRun());

  // Resetting the provider_host's waiting version should remove process refs
  // from the version.
  provider_host1_->UnassociateRegistration();
  ASSERT_FALSE(HasProcessToRun());
}

TEST_F(ServiceWorkerProviderHostTest,
       SetWaitingVersion_MultipleHostsForSameProcess) {
  provider_host1_->AssociateRegistration(registration_.get());
  provider_host2_->AssociateRegistration(registration_.get());
  ASSERT_TRUE(HasProcessToRun());

  // Associating version_ to two providers as waiting version.
  registration_->SetWaitingVersion(version_.get());
  ASSERT_TRUE(HasProcessToRun());

  // Disassociating one provider_host shouldn't remove all process refs
  // from the version yet.
  provider_host1_->UnassociateRegistration();
  ASSERT_TRUE(HasProcessToRun());

  // Disassociating the other provider_host will remove all process refs.
  provider_host2_->UnassociateRegistration();
  ASSERT_FALSE(HasProcessToRun());
}

TEST_F(ServiceWorkerProviderHostTest,
       ObserveVersionAttributesChanged_Basic) {
  provider_host1_->AssociateRegistration(registration_.get());
  provider_host2_->AssociateRegistration(registration_.get());
  VerifyVersionAttributes(provider_host1_, NULL, NULL, NULL);
  VerifyVersionAttributes(provider_host2_, NULL, NULL, NULL);

  registration_->SetInstallingVersion(version_.get());
  VerifyVersionAttributes(provider_host1_, version_.get(), NULL, NULL);
  VerifyVersionAttributes(provider_host2_, version_.get(), NULL, NULL);

  registration_->SetWaitingVersion(version_.get());
  VerifyVersionAttributes(provider_host1_, NULL, version_.get(), NULL);
  VerifyVersionAttributes(provider_host2_, NULL, version_.get(), NULL);

  // Disassociating the registration should clear all version attributes.
  provider_host2_->UnassociateRegistration();
  VerifyVersionAttributes(provider_host1_, NULL, version_.get(), NULL);
  VerifyVersionAttributes(provider_host2_, NULL, NULL, NULL);

  // Shouldn't notify the disassociated provider of the change.
  registration_->SetActiveVersion(version_.get());
  VerifyVersionAttributes(provider_host1_, NULL, NULL, version_.get());
  VerifyVersionAttributes(provider_host2_, NULL, NULL, NULL);
}

TEST_F(ServiceWorkerProviderHostTest,
       ObserveVersionAttributesChanged_MultipleVersions) {
  provider_host1_->AssociateRegistration(registration_.get());
  provider_host2_->AssociateRegistration(registration_.get());
  VerifyVersionAttributes(provider_host1_, NULL, NULL, NULL);
  VerifyVersionAttributes(provider_host2_, NULL, NULL, NULL);

  scoped_refptr<ServiceWorkerVersion> version1 = new ServiceWorkerVersion(
      registration_.get(), script_url_, 10L, context_->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> version2 = new ServiceWorkerVersion(
      registration_.get(), script_url_, 20L, context_->AsWeakPtr());

  registration_->SetInstallingVersion(version1.get());
  VerifyVersionAttributes(provider_host1_, version1.get(), NULL, NULL);
  VerifyVersionAttributes(provider_host2_, version1.get(), NULL, NULL);

  registration_->SetWaitingVersion(version1.get());
  VerifyVersionAttributes(provider_host1_, NULL, version1.get(), NULL);
  VerifyVersionAttributes(provider_host2_, NULL, version1.get(), NULL);

  registration_->SetInstallingVersion(version2.get());
  VerifyVersionAttributes(
      provider_host1_, version2.get(), version1.get(), NULL);
  VerifyVersionAttributes(
      provider_host2_, version2.get(), version1.get(), NULL);

  // Disassociating the registration should clear all version attributes.
  provider_host2_->UnassociateRegistration();
  VerifyVersionAttributes(
      provider_host1_, version2.get(), version1.get(), NULL);
  VerifyVersionAttributes(provider_host2_, NULL, NULL, NULL);

  // Shouldn't notify the disassociated provider of the change.
  registration_->SetActiveVersion(version1.get());
  VerifyVersionAttributes(
      provider_host1_, version2.get(), NULL, version1.get());
  VerifyVersionAttributes(provider_host2_, NULL, NULL, NULL);

  registration_->SetActiveVersion(version2.get());
  VerifyVersionAttributes(provider_host1_, NULL, NULL, version2.get());
  VerifyVersionAttributes(provider_host2_, NULL, NULL, NULL);
}

}  // namespace content
