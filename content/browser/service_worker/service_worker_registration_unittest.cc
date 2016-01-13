// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration.h"

#include <stdint.h>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration_handle.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerRegistrationTest : public testing::Test {
 public:
  ServiceWorkerRegistrationTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  void TearDown() override {
    helper_.reset();
    base::RunLoop().RunUntilIdle();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }

  class RegistrationListener : public ServiceWorkerRegistration::Listener {
   public:
    RegistrationListener() {}
    ~RegistrationListener() {
      if (observed_registration_.get())
        observed_registration_->RemoveListener(this);
    }

    void OnVersionAttributesChanged(
        ServiceWorkerRegistration* registration,
        ChangedVersionAttributesMask changed_mask,
        const ServiceWorkerRegistrationInfo& info) override {
      observed_registration_ = registration;
      observed_changed_mask_ = changed_mask;
      observed_info_ = info;
    }

    void OnRegistrationFailed(
        ServiceWorkerRegistration* registration) override {
      NOTREACHED();
    }

    void OnUpdateFound(ServiceWorkerRegistration* registration) override {
      NOTREACHED();
    }

    void Reset() {
      observed_registration_ = NULL;
      observed_changed_mask_ = ChangedVersionAttributesMask();
      observed_info_ = ServiceWorkerRegistrationInfo();
    }

    scoped_refptr<ServiceWorkerRegistration> observed_registration_;
    ChangedVersionAttributesMask observed_changed_mask_;
    ServiceWorkerRegistrationInfo observed_info_;
  };

 private:
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  TestBrowserThreadBundle thread_bundle_;
};

TEST_F(ServiceWorkerRegistrationTest, SetAndUnsetVersions) {
  const GURL kScope("http://www.example.not/");
  const GURL kScript("http://www.example.not/service_worker.js");
  int64_t kRegistrationId = 1L;
  scoped_refptr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(kScope, kRegistrationId,
                                    context()->AsWeakPtr());

  const int64_t version_1_id = 1L;
  const int64_t version_2_id = 2L;
  scoped_refptr<ServiceWorkerVersion> version_1 = new ServiceWorkerVersion(
      registration.get(), kScript, version_1_id, context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> version_2 = new ServiceWorkerVersion(
      registration.get(), kScript, version_2_id, context()->AsWeakPtr());

  RegistrationListener listener;
  registration->AddListener(&listener);
  registration->SetActiveVersion(version_1);

  EXPECT_EQ(version_1.get(), registration->active_version());
  EXPECT_EQ(registration, listener.observed_registration_);
  EXPECT_EQ(ChangedVersionAttributesMask::ACTIVE_VERSION,
            listener.observed_changed_mask_.changed());
  EXPECT_EQ(kScope, listener.observed_info_.pattern);
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(kScript, listener.observed_info_.active_version.script_url);
  EXPECT_EQ(listener.observed_info_.installing_version.version_id,
            kInvalidServiceWorkerVersionId);
  EXPECT_EQ(listener.observed_info_.waiting_version.version_id,
            kInvalidServiceWorkerVersionId);
  listener.Reset();

  registration->SetInstallingVersion(version_2);

  EXPECT_EQ(version_2.get(), registration->installing_version());
  EXPECT_EQ(ChangedVersionAttributesMask::INSTALLING_VERSION,
            listener.observed_changed_mask_.changed());
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(version_2_id,
            listener.observed_info_.installing_version.version_id);
  EXPECT_EQ(listener.observed_info_.waiting_version.version_id,
            kInvalidServiceWorkerVersionId);
  listener.Reset();

  registration->SetWaitingVersion(version_2);

  EXPECT_EQ(version_2.get(), registration->waiting_version());
  EXPECT_FALSE(registration->installing_version());
  EXPECT_TRUE(listener.observed_changed_mask_.waiting_changed());
  EXPECT_TRUE(listener.observed_changed_mask_.installing_changed());
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(version_2_id, listener.observed_info_.waiting_version.version_id);
  EXPECT_EQ(listener.observed_info_.installing_version.version_id,
            kInvalidServiceWorkerVersionId);
  listener.Reset();

  registration->UnsetVersion(version_2.get());

  EXPECT_FALSE(registration->waiting_version());
  EXPECT_EQ(ChangedVersionAttributesMask::WAITING_VERSION,
            listener.observed_changed_mask_.changed());
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(listener.observed_info_.waiting_version.version_id,
            kInvalidServiceWorkerVersionId);
  EXPECT_EQ(listener.observed_info_.installing_version.version_id,
            kInvalidServiceWorkerVersionId);
}

TEST_F(ServiceWorkerRegistrationTest, FailedRegistrationNoCrash) {
  const GURL kScope("http://www.example.not/");
  int64_t kRegistrationId = 1L;
  scoped_refptr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(kScope, kRegistrationId,
                                    context()->AsWeakPtr());
  scoped_ptr<ServiceWorkerRegistrationHandle> handle(
      new ServiceWorkerRegistrationHandle(
          context()->AsWeakPtr(), base::WeakPtr<ServiceWorkerProviderHost>(),
          registration.get()));
  registration->NotifyRegistrationFailed();
  // Don't crash when handle gets destructed.
}

}  // namespace content
