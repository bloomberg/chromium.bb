// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
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
    context_.reset(
        new ServiceWorkerContextCore(base::FilePath(),
                                     base::MessageLoopProxy::current(),
                                     base::MessageLoopProxy::current(),
                                     base::MessageLoopProxy::current(),
                                     NULL,
                                     NULL,
                                     NULL));

    scope_ = GURL("http://www.example.com/");
    script_url_ = GURL("http://www.example.com/service_worker.js");
    registration_ = new ServiceWorkerRegistration(
        scope_, script_url_, 1L, context_->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_,
        1L, context_->AsWeakPtr());

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
    context_.reset();
  }

  void SetActiveVersion(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ServiceWorkerVersion* version) {
    provider_host->SetActiveVersion(version);
  }

  void SetWaitingVersion(
      base::WeakPtr<ServiceWorkerProviderHost> provider_host,
      ServiceWorkerVersion* version) {
    provider_host->SetWaitingVersion(version);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<ServiceWorkerContextCore> context_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host1_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host2_;
  GURL scope_;
  GURL script_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHostTest);
};

TEST_F(ServiceWorkerProviderHostTest, SetActiveVersion_ProcessStatus) {
  ASSERT_FALSE(version_->HasProcessToRun());

  // Associating version_ to a provider_host's active version will internally
  // add the provider_host's process ref to the version.
  SetActiveVersion(provider_host1_, version_);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Re-associating the same version and provider_host should just work too.
  SetActiveVersion(provider_host1_, version_);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Resetting the provider_host's active version should remove process refs
  // from the version.
  SetActiveVersion(provider_host1_, NULL);
  ASSERT_FALSE(version_->HasProcessToRun());
}

TEST_F(ServiceWorkerProviderHostTest,
       SetActiveVersion_MultipleHostsForSameProcess) {
  ASSERT_FALSE(version_->HasProcessToRun());

  // Associating version_ to two providers as active version.
  SetActiveVersion(provider_host1_, version_);
  SetActiveVersion(provider_host2_, version_);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Disassociating one provider_host shouldn't remove all process refs
  // from the version yet.
  SetActiveVersion(provider_host1_, NULL);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Disassociating the other provider_host will remove all process refs.
  SetActiveVersion(provider_host2_, NULL);
  ASSERT_FALSE(version_->HasProcessToRun());
}

TEST_F(ServiceWorkerProviderHostTest, SetWaitingVersion_ProcessStatus) {
  ASSERT_FALSE(version_->HasProcessToRun());

  // Associating version_ to a provider_host's waiting version will internally
  // add the provider_host's process ref to the version.
  SetWaitingVersion(provider_host1_, version_);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Re-associating the same version and provider_host should just work too.
  SetWaitingVersion(provider_host1_, version_);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Resetting the provider_host's waiting version should remove process refs
  // from the version.
  SetWaitingVersion(provider_host1_, NULL);
  ASSERT_FALSE(version_->HasProcessToRun());
}

TEST_F(ServiceWorkerProviderHostTest,
       SetWaitingVersion_MultipleHostsForSameProcess) {
  ASSERT_FALSE(version_->HasProcessToRun());

  // Associating version_ to two providers as active version.
  SetWaitingVersion(provider_host1_, version_);
  SetWaitingVersion(provider_host2_, version_);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Disassociating one provider_host shouldn't remove all process refs
  // from the version yet.
  SetWaitingVersion(provider_host1_, NULL);
  ASSERT_TRUE(version_->HasProcessToRun());

  // Disassociating the other provider_host will remove all process refs.
  SetWaitingVersion(provider_host2_, NULL);
  ASSERT_FALSE(version_->HasProcessToRun());
}

}  // namespace content
