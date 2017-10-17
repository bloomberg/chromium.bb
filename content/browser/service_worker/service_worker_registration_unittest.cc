// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_registration.h"

#include <stdint.h>
#include <utility>

#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_dispatcher_host.h"
#include "content/browser/service_worker/service_worker_registration_handle.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/service_worker_registration.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

// From service_worker_registration.cc.
constexpr base::TimeDelta kMaxLameDuckTime = base::TimeDelta::FromMinutes(5);

int CreateInflightRequest(ServiceWorkerVersion* version) {
  version->StartWorker(ServiceWorkerMetrics::EventType::PUSH,
                       base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  base::RunLoop().RunUntilIdle();
  return version->StartRequest(
      ServiceWorkerMetrics::EventType::PUSH,
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
}

}  // namespace

class ServiceWorkerRegistrationTest : public testing::Test {
 public:
  ServiceWorkerRegistrationTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));

    context()->storage()->LazyInitializeForTest(
        base::BindOnce(&base::DoNothing));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    helper_.reset();
    base::RunLoop().RunUntilIdle();
  }

  ServiceWorkerContextCore* context() { return helper_->context(); }
  ServiceWorkerStorage* storage() { return helper_->context()->storage(); }

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

 protected:
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  TestBrowserThreadBundle thread_bundle_;
};

TEST_F(ServiceWorkerRegistrationTest, SetAndUnsetVersions) {
  const GURL kScope("http://www.example.not/");
  const GURL kScript("http://www.example.not/service_worker.js");
  int64_t kRegistrationId = 1L;
  scoped_refptr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(
          blink::mojom::ServiceWorkerRegistrationOptions(kScope),
          kRegistrationId, context()->AsWeakPtr());

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
  auto registration = base::MakeRefCounted<ServiceWorkerRegistration>(
      blink::mojom::ServiceWorkerRegistrationOptions(kScope), kRegistrationId,
      context()->AsWeakPtr());
  auto dispatcher_host = base::MakeRefCounted<ServiceWorkerDispatcherHost>(
      helper_->mock_render_process_id(),
      helper_->browser_context()->GetResourceContext());
  // ServiceWorkerRegistrationHandle ctor will make |handle| be owned by
  // |dispatcher_host|.
  auto* handle = new ServiceWorkerRegistrationHandle(
      context()->AsWeakPtr(), dispatcher_host.get(),
      base::WeakPtr<ServiceWorkerProviderHost>(), registration.get());
  ALLOW_UNUSED_LOCAL(handle);
  registration->NotifyRegistrationFailed();
  // Don't crash when handle gets destructed.
}

TEST_F(ServiceWorkerRegistrationTest, NavigationPreload) {
  const GURL kScope("http://www.example.not/");
  const GURL kScript("https://www.example.not/service_worker.js");
  // Setup.
  scoped_refptr<ServiceWorkerRegistration> registration =
      new ServiceWorkerRegistration(
          blink::mojom::ServiceWorkerRegistrationOptions(kScope),
          storage()->NewRegistrationId(), context()->AsWeakPtr());
  scoped_refptr<ServiceWorkerVersion> version_1 = new ServiceWorkerVersion(
      registration.get(), kScript, storage()->NewVersionId(),
      context()->AsWeakPtr());
  version_1->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  registration->SetActiveVersion(version_1);
  version_1->SetStatus(ServiceWorkerVersion::ACTIVATED);
  scoped_refptr<ServiceWorkerVersion> version_2 = new ServiceWorkerVersion(
      registration.get(), kScript, storage()->NewVersionId(),
      context()->AsWeakPtr());
  version_2->set_fetch_handler_existence(
      ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
  registration->SetWaitingVersion(version_2);
  version_2->SetStatus(ServiceWorkerVersion::INSTALLED);

  // Navigation preload is disabled by default.
  EXPECT_FALSE(version_1->navigation_preload_state().enabled);
  // Enabling it sets the flag on the active version.
  registration->EnableNavigationPreload(true);
  EXPECT_TRUE(version_1->navigation_preload_state().enabled);
  // A new active version gets the flag.
  registration->SetActiveVersion(version_2);
  version_2->SetStatus(ServiceWorkerVersion::ACTIVATING);
  EXPECT_TRUE(version_2->navigation_preload_state().enabled);
  // Disabling it unsets the flag on the active version.
  registration->EnableNavigationPreload(false);
  EXPECT_FALSE(version_2->navigation_preload_state().enabled);
}

// Sets up a registration with a waiting worker, and an active worker
// with a controllee and an inflight request.
class ServiceWorkerActivationTest : public ServiceWorkerRegistrationTest {
 public:
  ServiceWorkerActivationTest() : ServiceWorkerRegistrationTest() {}

  void SetUp() override {
    ServiceWorkerRegistrationTest::SetUp();

    const GURL kScope("https://www.example.not/");
    const GURL kScript("https://www.example.not/service_worker.js");

    registration_ = new ServiceWorkerRegistration(
        blink::mojom::ServiceWorkerRegistrationOptions(kScope),
        storage()->NewRegistrationId(), context()->AsWeakPtr());

    // Create an active version.
    scoped_refptr<ServiceWorkerVersion> version_1 = new ServiceWorkerVersion(
        registration_.get(), kScript, storage()->NewVersionId(),
        context()->AsWeakPtr());
    version_1->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    registration_->SetActiveVersion(version_1);
    version_1->SetStatus(ServiceWorkerVersion::ACTIVATED);

    // Store the registration.
    std::vector<ServiceWorkerDatabase::ResourceRecord> records_1;
    records_1.push_back(WriteToDiskCacheSync(
        helper_->context()->storage(), version_1->script_url(),
        helper_->context()->storage()->NewResourceId(), {} /* headers */,
        "I'm the body", "I'm the meta data"));
    version_1->script_cache_map()->SetResources(records_1);
    version_1->SetMainScriptHttpResponseInfo(
        EmbeddedWorkerTestHelper::CreateHttpResponseInfo());
    ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_MAX_VALUE;
    context()->storage()->StoreRegistration(
        registration_.get(), version_1.get(),
        CreateReceiverOnCurrentThread(&status));
    base::RunLoop().RunUntilIdle();
    ASSERT_EQ(SERVICE_WORKER_OK, status);

    // Give the active version a controllee.
    host_ = CreateProviderHostForWindow(
        helper_->mock_render_process_id(), 1 /* dummy provider_id */,
        true /* is_parent_frame_secure */, context()->AsWeakPtr(),
        &remote_endpoint_);
    DCHECK(remote_endpoint_.client_request()->is_pending());
    DCHECK(remote_endpoint_.host_ptr()->is_bound());
    version_1->AddControllee(host_.get());

    // Give the active version an in-flight request.
    inflight_request_id_ = CreateInflightRequest(version_1.get());

    // Create a waiting version.
    scoped_refptr<ServiceWorkerVersion> version_2 = new ServiceWorkerVersion(
        registration_.get(), kScript, storage()->NewVersionId(),
        context()->AsWeakPtr());
    std::vector<ServiceWorkerDatabase::ResourceRecord> records_2;
    records_2.push_back(WriteToDiskCacheSync(
        helper_->context()->storage(), version_2->script_url(),
        helper_->context()->storage()->NewResourceId(), {} /* headers */,
        "I'm the body", "I'm the meta data"));
    version_2->script_cache_map()->SetResources(records_2);
    version_2->SetMainScriptHttpResponseInfo(
        EmbeddedWorkerTestHelper::CreateHttpResponseInfo());
    version_2->set_fetch_handler_existence(
        ServiceWorkerVersion::FetchHandlerExistence::EXISTS);
    registration_->SetWaitingVersion(version_2);
    version_2->StartWorker(ServiceWorkerMetrics::EventType::INSTALL,
                           base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
    version_2->SetStatus(ServiceWorkerVersion::INSTALLED);

    // Set it to activate when ready. The original version should still be
    // active.
    registration_->ActivateWaitingVersionWhenReady();
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(version_1.get(), registration_->active_version());
  }

  void TearDown() override {
    registration_->active_version()->RemoveListener(registration_.get());
    ServiceWorkerRegistrationTest::TearDown();
  }

  ServiceWorkerRegistration* registration() { return registration_.get(); }
  ServiceWorkerProviderHost* controllee() { return host_.get(); }
  int inflight_request_id() const { return inflight_request_id_; }

  bool IsLameDuckTimerRunning() {
    return registration_->lame_duck_timer_.IsRunning();
  }

  void RunLameDuckTimer() { registration_->RemoveLameDuckIfNeeded(); }

  void SimulateSkipWaiting(ServiceWorkerVersion* version, int request_id) {
    version->OnSkipWaiting(request_id);
  }

 private:
  scoped_refptr<ServiceWorkerRegistration> registration_;
  std::unique_ptr<ServiceWorkerProviderHost> host_;
  ServiceWorkerRemoteProviderEndpoint remote_endpoint_;
  int inflight_request_id_ = -1;
};

// Test activation triggered by finishing all requests.
TEST_F(ServiceWorkerActivationTest, NoInflightRequest) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();

  // Remove the controllee. Since there is an in-flight request,
  // activation should not yet happen.
  version_1->RemoveControllee(controllee());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());

  // Finish the request. Activation should happen.
  version_1->FinishRequest(inflight_request_id(), true /* was_handled */,
                           base::Time::Now());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_2.get(), reg->active_version());
}

// Test activation triggered by loss of controllee.
TEST_F(ServiceWorkerActivationTest, NoControllee) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();

  // Finish the request. Since there is a controllee, activation should not yet
  // happen.
  version_1->FinishRequest(inflight_request_id(), true /* was_handled */,
                           base::Time::Now());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());

  // Remove the controllee. Activation should happen.
  version_1->RemoveControllee(controllee());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_2.get(), reg->active_version());
}

// Test activation triggered by skipWaiting.
TEST_F(ServiceWorkerActivationTest, SkipWaiting) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();

  // Finish the in-flight request. Since there is a controllee,
  // activation should not happen.
  version_1->FinishRequest(inflight_request_id(), true /* was_handled */,
                           base::Time::Now());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());

  // Call skipWaiting. Activation should happen.
  SimulateSkipWaiting(version_2.get(), 77 /* dummy request_id */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_2.get(), reg->active_version());
}

// Test activation triggered by skipWaiting and finishing requests.
TEST_F(ServiceWorkerActivationTest, SkipWaitingWithInflightRequest) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();

  // Set skip waiting flag. Since there is still an in-flight request,
  // activation should not happen.
  SimulateSkipWaiting(version_2.get(), 77 /* dummy request_id */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());

  // Finish the request. Activation should happen.
  version_1->FinishRequest(inflight_request_id(), true /* was_handled */,
                           base::Time::Now());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_2.get(), reg->active_version());
}

TEST_F(ServiceWorkerActivationTest, TimeSinceSkipWaiting_Installing) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version = reg->waiting_version();
  base::SimpleTestTickClock* clock = new base::SimpleTestTickClock();
  clock->SetNowTicks(base::TimeTicks::Now());
  version->SetTickClockForTesting(base::WrapUnique(clock));

  // Reset version to the installing phase.
  reg->UnsetVersion(version.get());
  version->SetStatus(ServiceWorkerVersion::INSTALLING);

  // Call skipWaiting(). The time ticks since skip waiting shouldn't start
  // since the version is not yet installed.
  SimulateSkipWaiting(version.get(), 77 /* dummy request_id */);
  base::RunLoop().RunUntilIdle();
  clock->Advance(base::TimeDelta::FromSeconds(11));
  EXPECT_EQ(base::TimeDelta(), version->TimeSinceSkipWaiting());

  // Install the version. Now the skip waiting time starts ticking.
  version->SetStatus(ServiceWorkerVersion::INSTALLED);
  reg->SetWaitingVersion(version);
  base::RunLoop().RunUntilIdle();
  clock->Advance(base::TimeDelta::FromSeconds(33));
  EXPECT_EQ(base::TimeDelta::FromSeconds(33), version->TimeSinceSkipWaiting());

  // Call skipWaiting() again. It doesn't reset the time.
  SimulateSkipWaiting(version.get(), 88 /* dummy request_id */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::TimeDelta::FromSeconds(33), version->TimeSinceSkipWaiting());
}

// Test lame duck timer triggered by skip waiting.
TEST_F(ServiceWorkerActivationTest, LameDuckTime_SkipWaiting) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();
  base::SimpleTestTickClock* clock_1 = new base::SimpleTestTickClock();
  base::SimpleTestTickClock* clock_2 = new base::SimpleTestTickClock();
  clock_1->SetNowTicks(base::TimeTicks::Now());
  clock_2->SetNowTicks(clock_1->NowTicks());
  version_1->SetTickClockForTesting(base::WrapUnique(clock_1));
  version_2->SetTickClockForTesting(base::WrapUnique(clock_2));

  // Set skip waiting flag. Since there is still an in-flight request,
  // activation should not happen. But the lame duck timer should start.
  EXPECT_FALSE(IsLameDuckTimerRunning());
  SimulateSkipWaiting(version_2.get(), 77 /* dummy request_id */);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Move forward by lame duck time.
  clock_2->Advance(kMaxLameDuckTime + base::TimeDelta::FromSeconds(1));

  // Activation should happen by the lame duck timer.
  RunLameDuckTimer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_2.get(), reg->active_version());
  EXPECT_FALSE(IsLameDuckTimerRunning());
}

// Test lame duck timer triggered by loss of controllee.
TEST_F(ServiceWorkerActivationTest, LameDuckTime_NoControllee) {
  scoped_refptr<ServiceWorkerRegistration> reg = registration();
  scoped_refptr<ServiceWorkerVersion> version_1 = reg->active_version();
  scoped_refptr<ServiceWorkerVersion> version_2 = reg->waiting_version();
  base::SimpleTestTickClock* clock_1 = new base::SimpleTestTickClock();
  base::SimpleTestTickClock* clock_2 = new base::SimpleTestTickClock();
  clock_1->SetNowTicks(base::TimeTicks::Now());
  clock_2->SetNowTicks(clock_1->NowTicks());
  version_1->SetTickClockForTesting(base::WrapUnique(clock_1));
  version_2->SetTickClockForTesting(base::WrapUnique(clock_2));

  // Remove the controllee. Since there is still an in-flight request,
  // activation should not happen. But the lame duck timer should start.
  EXPECT_FALSE(IsLameDuckTimerRunning());
  version_1->RemoveControllee(controllee());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Move clock forward by a little bit.
  constexpr base::TimeDelta kLittleBit = base::TimeDelta::FromMinutes(1);
  clock_1->Advance(kLittleBit);

  // Add a controllee again to reset the lame duck period.
  version_1->AddControllee(controllee());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Remove the controllee.
  version_1->RemoveControllee(controllee());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Move clock forward to the next lame duck timer tick.
  clock_1->Advance(kMaxLameDuckTime - kLittleBit +
                   base::TimeDelta::FromSeconds(1));

  // Run the lame duck timer. Activation should not yet happen
  // since the lame duck period has not expired.
  RunLameDuckTimer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_1.get(), reg->active_version());
  EXPECT_TRUE(IsLameDuckTimerRunning());

  // Continue on to the next lame duck timer tick.
  clock_1->Advance(kMaxLameDuckTime + base::TimeDelta::FromSeconds(1));

  // Activation should happen by the lame duck timer.
  RunLameDuckTimer();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(version_2.get(), reg->active_version());
  EXPECT_FALSE(IsLameDuckTimerRunning());
}

}  // namespace content
