// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerRegistrationTest : public testing::Test {
 public:
  ServiceWorkerRegistrationTest()
      : io_thread_(BrowserThread::IO, &message_loop_) {}

  virtual void SetUp() OVERRIDE {
    context_.reset(
        new ServiceWorkerContextCore(base::FilePath(),
                                     base::ThreadTaskRunnerHandle::Get(),
                                     base::ThreadTaskRunnerHandle::Get(),
                                     base::ThreadTaskRunnerHandle::Get(),
                                     NULL,
                                     NULL,
                                     NULL));
    context_ptr_ = context_->AsWeakPtr();
  }

  virtual void TearDown() OVERRIDE {
    context_.reset();
    base::RunLoop().RunUntilIdle();
  }

  class RegistrationListener : public ServiceWorkerRegistration::Listener {
   public:
    RegistrationListener() {}
    ~RegistrationListener() {
      if (observed_registration_)
        observed_registration_->RemoveListener(this);
    }

    virtual void OnVersionAttributesChanged(
        ServiceWorkerRegistration* registration,
        ChangedVersionAttributesMask changed_mask,
        const ServiceWorkerRegistrationInfo& info) OVERRIDE {
      observed_registration_ = registration;
      observed_changed_mask_ = changed_mask;
      observed_info_ = info;
    }

    virtual void OnRegistrationFailed(
        ServiceWorkerRegistration* registration) OVERRIDE {
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

 protected:
  scoped_ptr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerContextCore> context_ptr_;
  base::MessageLoopForIO message_loop_;
  BrowserThreadImpl io_thread_;
};

TEST_F(ServiceWorkerRegistrationTest, SetAndUnsetVersions) {
  const GURL kScope("http://www.example.not/");
  const GURL kScript("http://www.example.not/service_worker.js");
  int64 kRegistrationId = 1L;
  scoped_refptr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(
          kScope,
          kScript,
          kRegistrationId,
          context_ptr_);

  const int64 version_1_id = 1L;
  const int64 version_2_id = 2L;
  scoped_refptr<ServiceWorkerVersion> version_1 =
      new ServiceWorkerVersion(registration, version_1_id, context_ptr_);
  scoped_refptr<ServiceWorkerVersion> version_2 =
      new ServiceWorkerVersion(registration, version_2_id, context_ptr_);

  RegistrationListener listener;
  registration->AddListener(&listener);
  registration->SetActiveVersion(version_1);

  EXPECT_EQ(version_1, registration->active_version());
  EXPECT_EQ(registration, listener.observed_registration_);
  EXPECT_EQ(ChangedVersionAttributesMask::ACTIVE_VERSION,
            listener.observed_changed_mask_.changed());
  EXPECT_EQ(kScope, listener.observed_info_.pattern);
  EXPECT_EQ(kScript, listener.observed_info_.script_url);
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_TRUE(listener.observed_info_.installing_version.is_null);
  EXPECT_TRUE(listener.observed_info_.waiting_version.is_null);
  EXPECT_TRUE(listener.observed_info_.controlling_version.is_null);
  listener.Reset();

  registration->SetInstallingVersion(version_2);

  EXPECT_EQ(version_2, registration->installing_version());
  EXPECT_EQ(ChangedVersionAttributesMask::INSTALLING_VERSION,
            listener.observed_changed_mask_.changed());
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(version_2_id,
            listener.observed_info_.installing_version.version_id);
  EXPECT_TRUE(listener.observed_info_.waiting_version.is_null);
  EXPECT_TRUE(listener.observed_info_.controlling_version.is_null);
  listener.Reset();

  registration->SetWaitingVersion(version_2);

  EXPECT_EQ(version_2, registration->waiting_version());
  EXPECT_FALSE(registration->installing_version());
  EXPECT_TRUE(listener.observed_changed_mask_.waiting_changed());
  EXPECT_TRUE(listener.observed_changed_mask_.installing_changed());
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_EQ(version_2_id, listener.observed_info_.waiting_version.version_id);
  EXPECT_TRUE(listener.observed_info_.installing_version.is_null);
  EXPECT_TRUE(listener.observed_info_.controlling_version.is_null);
  listener.Reset();

  registration->UnsetVersion(version_2);

  EXPECT_FALSE(registration->waiting_version());
  EXPECT_EQ(ChangedVersionAttributesMask::WAITING_VERSION,
            listener.observed_changed_mask_.changed());
  EXPECT_EQ(version_1_id, listener.observed_info_.active_version.version_id);
  EXPECT_TRUE(listener.observed_info_.waiting_version.is_null);
  EXPECT_TRUE(listener.observed_info_.installing_version.is_null);
  EXPECT_TRUE(listener.observed_info_.controlling_version.is_null);
}

TEST_F(ServiceWorkerRegistrationTest, FailedRegistrationNoCrash) {
  const GURL kScope("http://www.example.not/");
  const GURL kScript("http://www.example.not/service_worker.js");
  int64 kRegistrationId = 1L;
  int kProviderId = 1;
  scoped_refptr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(
          kScope,
          kScript,
          kRegistrationId,
          context_ptr_);
  scoped_ptr<ServiceWorkerRegistrationHandle> handle(
      new ServiceWorkerRegistrationHandle(context_ptr_,
                                          NULL,
                                          kProviderId,
                                          registration.get()));
  registration->NotifyRegistrationFailed();
  // Don't crash when handle gets destructed.
}

}  // namespace content
