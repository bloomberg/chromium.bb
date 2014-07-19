// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_test_sink.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::IOBuffer;
using net::TestCompletionCallback;
using net::WrappedIOBuffer;

// Unit tests for testing all job registration tasks.
namespace content {

namespace {

int kMockRenderProcessId = 88;

void SaveRegistrationCallback(
    ServiceWorkerStatusCode expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration_out,
    ServiceWorkerStatusCode status,
    ServiceWorkerRegistration* registration,
    ServiceWorkerVersion* version) {
  ASSERT_TRUE(!version || version->registration_id() == registration->id())
      << version << " " << registration;
  EXPECT_EQ(expected_status, status);
  *called = true;
  *registration_out = registration;
}

void SaveFoundRegistrationCallback(
    ServiceWorkerStatusCode expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& result) {
  EXPECT_EQ(expected_status, status);
  *called = true;
  *registration = result;
}

// Creates a callback which both keeps track of if it's been called,
// as well as the resulting registration. Whent the callback is fired,
// it ensures that the resulting status matches the expectation.
// 'called' is useful for making sure a sychronous callback is or
// isn't called.
ServiceWorkerRegisterJob::RegistrationCallback SaveRegistration(
    ServiceWorkerStatusCode expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration) {
  *called = false;
  return base::Bind(
      &SaveRegistrationCallback, expected_status, called, registration);
}

ServiceWorkerStorage::FindRegistrationCallback SaveFoundRegistration(
    ServiceWorkerStatusCode expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration) {
  *called = false;
  return base::Bind(&SaveFoundRegistrationCallback,
                    expected_status,
                    called,
                    registration);
}

void SaveUnregistrationCallback(ServiceWorkerStatusCode expected_status,
                                bool* called,
                                ServiceWorkerStatusCode status) {
  EXPECT_EQ(expected_status, status);
  *called = true;
}

ServiceWorkerUnregisterJob::UnregistrationCallback SaveUnregistration(
    ServiceWorkerStatusCode expected_status,
    bool* called) {
  *called = false;
  return base::Bind(&SaveUnregistrationCallback, expected_status, called);
}

}  // namespace

class ServiceWorkerJobTest : public testing::Test {
 public:
  ServiceWorkerJobTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        render_process_id_(kMockRenderProcessId) {}

  virtual void SetUp() OVERRIDE {
    helper_.reset(new EmbeddedWorkerTestHelper(render_process_id_));
  }

  virtual void TearDown() OVERRIDE {
    helper_.reset();
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }

  ServiceWorkerJobCoordinator* job_coordinator() const {
    return context()->job_coordinator();
  }
  ServiceWorkerStorage* storage() const { return context()->storage(); }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<EmbeddedWorkerTestHelper> helper_;
  int render_process_id_;
};

TEST_F(ServiceWorkerJobTest, SameDocumentSameRegistration) {
  scoped_refptr<ServiceWorkerRegistration> original_registration;
  bool called;
  job_coordinator()->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &original_registration));
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> registration1;
  storage()->FindRegistrationForDocument(
      GURL("http://www.example.com/"),
      SaveFoundRegistration(SERVICE_WORKER_OK, &called, &registration1));
  scoped_refptr<ServiceWorkerRegistration> registration2;
  storage()->FindRegistrationForDocument(
      GURL("http://www.example.com/"),
      SaveFoundRegistration(SERVICE_WORKER_OK, &called, &registration2));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  ASSERT_TRUE(registration1);
  ASSERT_EQ(registration1, original_registration);
  ASSERT_EQ(registration1, registration2);
}

TEST_F(ServiceWorkerJobTest, SameMatchSameRegistration) {
  bool called;
  scoped_refptr<ServiceWorkerRegistration> original_registration;
  job_coordinator()->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &original_registration));
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  ASSERT_NE(static_cast<ServiceWorkerRegistration*>(NULL),
            original_registration.get());

  scoped_refptr<ServiceWorkerRegistration> registration1;
  storage()->FindRegistrationForDocument(
      GURL("http://www.example.com/one"),
      SaveFoundRegistration(SERVICE_WORKER_OK, &called, &registration1));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> registration2;
  storage()->FindRegistrationForDocument(
      GURL("http://www.example.com/two"),
      SaveFoundRegistration(SERVICE_WORKER_OK, &called, &registration2));
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  ASSERT_EQ(registration1, original_registration);
  ASSERT_EQ(registration1, registration2);
}

TEST_F(ServiceWorkerJobTest, DifferentMatchDifferentRegistration) {
  bool called1;
  scoped_refptr<ServiceWorkerRegistration> original_registration1;
  job_coordinator()->Register(
      GURL("http://www.example.com/one/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called1, &original_registration1));

  bool called2;
  scoped_refptr<ServiceWorkerRegistration> original_registration2;
  job_coordinator()->Register(
      GURL("http://www.example.com/two/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called2, &original_registration2));

  EXPECT_FALSE(called1);
  EXPECT_FALSE(called2);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called2);
  EXPECT_TRUE(called1);

  scoped_refptr<ServiceWorkerRegistration> registration1;
  storage()->FindRegistrationForDocument(
      GURL("http://www.example.com/one/"),
      SaveFoundRegistration(SERVICE_WORKER_OK, &called1, &registration1));
  scoped_refptr<ServiceWorkerRegistration> registration2;
  storage()->FindRegistrationForDocument(
      GURL("http://www.example.com/two/"),
      SaveFoundRegistration(SERVICE_WORKER_OK, &called2, &registration2));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called2);
  EXPECT_TRUE(called1);
  ASSERT_NE(registration1, registration2);
}

// Make sure basic registration is working.
TEST_F(ServiceWorkerJobTest, Register) {
  bool called = false;
  scoped_refptr<ServiceWorkerRegistration> registration;
  job_coordinator()->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_NE(scoped_refptr<ServiceWorkerRegistration>(NULL), registration);
}

// Make sure registrations are cleaned up when they are unregistered.
TEST_F(ServiceWorkerJobTest, Unregister) {
  GURL pattern("http://www.example.com/*");

  bool called;
  scoped_refptr<ServiceWorkerRegistration> registration;
  job_coordinator()->Register(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  job_coordinator()->Unregister(pattern,
                                SaveUnregistration(SERVICE_WORKER_OK, &called));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_TRUE(registration->HasOneRef());

  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(SERVICE_WORKER_ERROR_NOT_FOUND,
                            &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(scoped_refptr<ServiceWorkerRegistration>(NULL), registration);
}

TEST_F(ServiceWorkerJobTest, Unregister_NothingRegistered) {
  GURL pattern("http://www.example.com/*");

  bool called;
  job_coordinator()->Unregister(pattern,
                                SaveUnregistration(SERVICE_WORKER_OK, &called));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
}

// Make sure that when a new registration replaces an existing
// registration, that the old one is cleaned up.
TEST_F(ServiceWorkerJobTest, RegisterNewScript) {
  GURL pattern("http://www.example.com/*");

  bool called;
  scoped_refptr<ServiceWorkerRegistration> old_registration;
  job_coordinator()->Register(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &old_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> old_registration_by_pattern;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_OK, &called, &old_registration_by_pattern));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(old_registration, old_registration_by_pattern);
  old_registration_by_pattern = NULL;

  scoped_refptr<ServiceWorkerRegistration> new_registration;
  job_coordinator()->Register(
      pattern,
      GURL("http://www.example.com/service_worker_new.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &new_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_TRUE(old_registration->HasOneRef());

  ASSERT_NE(old_registration, new_registration);

  scoped_refptr<ServiceWorkerRegistration> new_registration_by_pattern;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_OK, &called, &new_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_NE(new_registration_by_pattern, old_registration);
}

// Make sure that when registering a duplicate pattern+script_url
// combination, that the same registration is used.
TEST_F(ServiceWorkerJobTest, RegisterDuplicateScript) {
  GURL pattern("http://www.example.com/*");
  GURL script_url("http://www.example.com/service_worker.js");

  bool called;
  scoped_refptr<ServiceWorkerRegistration> old_registration;
  job_coordinator()->Register(
      pattern,
      script_url,
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &old_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> old_registration_by_pattern;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_OK, &called, &old_registration_by_pattern));
  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_TRUE(old_registration_by_pattern);

  scoped_refptr<ServiceWorkerRegistration> new_registration;
  job_coordinator()->Register(
      pattern,
      script_url,
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &new_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(old_registration, new_registration);

  ASSERT_FALSE(old_registration->HasOneRef());

  scoped_refptr<ServiceWorkerRegistration> new_registration_by_pattern;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_OK, &called, &new_registration_by_pattern));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(new_registration, old_registration);
}

class FailToStartWorkerTestHelper : public EmbeddedWorkerTestHelper {
 public:
  explicit FailToStartWorkerTestHelper(int mock_render_process_id)
      : EmbeddedWorkerTestHelper(mock_render_process_id) {}

  virtual void OnStartWorker(int embedded_worker_id,
                             int64 service_worker_version_id,
                             const GURL& scope,
                             const GURL& script_url,
                             bool pause_after_download) OVERRIDE {
    EmbeddedWorkerInstance* worker = registry()->GetWorker(embedded_worker_id);
    registry()->OnWorkerStopped(worker->process_id(), embedded_worker_id);
  }
};

TEST_F(ServiceWorkerJobTest, Register_FailToStartWorker) {
  helper_.reset(new FailToStartWorkerTestHelper(render_process_id_));

  bool called = false;
  scoped_refptr<ServiceWorkerRegistration> registration;
  job_coordinator()->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(
          SERVICE_WORKER_ERROR_START_WORKER_FAILED, &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(called);
  ASSERT_EQ(scoped_refptr<ServiceWorkerRegistration>(NULL), registration);
}

// Register and then unregister the pattern, in parallel. Job coordinator should
// process jobs until the last job.
TEST_F(ServiceWorkerJobTest, ParallelRegUnreg) {
  GURL pattern("http://www.example.com/*");
  GURL script_url("http://www.example.com/service_worker.js");

  bool registration_called = false;
  scoped_refptr<ServiceWorkerRegistration> registration;
  job_coordinator()->Register(
      pattern,
      script_url,
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &registration_called, &registration));

  bool unregistration_called = false;
  job_coordinator()->Unregister(
      pattern,
      SaveUnregistration(SERVICE_WORKER_OK, &unregistration_called));

  ASSERT_FALSE(registration_called);
  ASSERT_FALSE(unregistration_called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(registration_called);
  ASSERT_TRUE(unregistration_called);

  bool find_called = false;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_ERROR_NOT_FOUND, &find_called, &registration));

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration);
}

// Register conflicting scripts for the same pattern. The most recent
// registration should win, and the old registration should have been
// shutdown.
TEST_F(ServiceWorkerJobTest, ParallelRegNewScript) {
  GURL pattern("http://www.example.com/*");

  GURL script_url1("http://www.example.com/service_worker1.js");
  bool registration1_called = false;
  scoped_refptr<ServiceWorkerRegistration> registration1;
  job_coordinator()->Register(
      pattern,
      script_url1,
      render_process_id_,
      SaveRegistration(
          SERVICE_WORKER_OK, &registration1_called, &registration1));

  GURL script_url2("http://www.example.com/service_worker2.js");
  bool registration2_called = false;
  scoped_refptr<ServiceWorkerRegistration> registration2;
  job_coordinator()->Register(
      pattern,
      script_url2,
      render_process_id_,
      SaveRegistration(
          SERVICE_WORKER_OK, &registration2_called, &registration2));

  ASSERT_FALSE(registration1_called);
  ASSERT_FALSE(registration2_called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(registration1_called);
  ASSERT_TRUE(registration2_called);

  scoped_refptr<ServiceWorkerRegistration> registration;
  bool find_called = false;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_OK, &find_called, &registration));

  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(registration2, registration);
}

// Register the exact same pattern + script. Requests should be
// coalesced such that both callers get the exact same registration
// object.
TEST_F(ServiceWorkerJobTest, ParallelRegSameScript) {
  GURL pattern("http://www.example.com/*");

  GURL script_url("http://www.example.com/service_worker1.js");
  bool registration1_called = false;
  scoped_refptr<ServiceWorkerRegistration> registration1;
  job_coordinator()->Register(
      pattern,
      script_url,
      render_process_id_,
      SaveRegistration(
          SERVICE_WORKER_OK, &registration1_called, &registration1));

  bool registration2_called = false;
  scoped_refptr<ServiceWorkerRegistration> registration2;
  job_coordinator()->Register(
      pattern,
      script_url,
      render_process_id_,
      SaveRegistration(
          SERVICE_WORKER_OK, &registration2_called, &registration2));

  ASSERT_FALSE(registration1_called);
  ASSERT_FALSE(registration2_called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(registration1_called);
  ASSERT_TRUE(registration2_called);

  ASSERT_EQ(registration1, registration2);

  scoped_refptr<ServiceWorkerRegistration> registration;
  bool find_called = false;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_OK, &find_called, &registration));

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(registration, registration1);
}

// Call simulataneous unregister calls.
TEST_F(ServiceWorkerJobTest, ParallelUnreg) {
  GURL pattern("http://www.example.com/*");

  GURL script_url("http://www.example.com/service_worker.js");
  bool unregistration1_called = false;
  job_coordinator()->Unregister(
      pattern,
      SaveUnregistration(SERVICE_WORKER_OK, &unregistration1_called));

  bool unregistration2_called = false;
  job_coordinator()->Unregister(
      pattern, SaveUnregistration(SERVICE_WORKER_OK, &unregistration2_called));

  ASSERT_FALSE(unregistration1_called);
  ASSERT_FALSE(unregistration2_called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(unregistration1_called);
  ASSERT_TRUE(unregistration2_called);

  // There isn't really a way to test that they are being coalesced,
  // but we can make sure they can exist simultaneously without
  // crashing.
  scoped_refptr<ServiceWorkerRegistration> registration;
  bool find_called = false;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_ERROR_NOT_FOUND, &find_called, &registration));

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration);
}

TEST_F(ServiceWorkerJobTest, AbortAll_Register) {
  GURL pattern1("http://www1.example.com/*");
  GURL pattern2("http://www2.example.com/*");
  GURL script_url1("http://www1.example.com/service_worker.js");
  GURL script_url2("http://www2.example.com/service_worker.js");

  bool registration_called1 = false;
  scoped_refptr<ServiceWorkerRegistration> registration1;
  job_coordinator()->Register(
      pattern1,
      script_url1,
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_ERROR_ABORT,
                       &registration_called1, &registration1));

  bool registration_called2 = false;
  scoped_refptr<ServiceWorkerRegistration> registration2;
  job_coordinator()->Register(
      pattern2,
      script_url2,
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_ERROR_ABORT,
                       &registration_called2, &registration2));

  ASSERT_FALSE(registration_called1);
  ASSERT_FALSE(registration_called2);
  job_coordinator()->AbortAll();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(registration_called1);
  ASSERT_TRUE(registration_called2);

  bool find_called1 = false;
  storage()->FindRegistrationForPattern(
      pattern1,
      SaveFoundRegistration(
          SERVICE_WORKER_ERROR_NOT_FOUND, &find_called1, &registration1));

  bool find_called2 = false;
  storage()->FindRegistrationForPattern(
      pattern2,
      SaveFoundRegistration(
          SERVICE_WORKER_ERROR_NOT_FOUND, &find_called2, &registration2));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(find_called1);
  ASSERT_TRUE(find_called2);
  EXPECT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration1);
  EXPECT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration2);
}

TEST_F(ServiceWorkerJobTest, AbortAll_Unregister) {
  GURL pattern1("http://www1.example.com/*");
  GURL pattern2("http://www2.example.com/*");

  bool unregistration_called1 = false;
  scoped_refptr<ServiceWorkerRegistration> registration1;
  job_coordinator()->Unregister(
      pattern1,
      SaveUnregistration(SERVICE_WORKER_ERROR_ABORT,
                         &unregistration_called1));

  bool unregistration_called2 = false;
  job_coordinator()->Unregister(
      pattern2,
      SaveUnregistration(SERVICE_WORKER_ERROR_ABORT,
                         &unregistration_called2));

  ASSERT_FALSE(unregistration_called1);
  ASSERT_FALSE(unregistration_called2);
  job_coordinator()->AbortAll();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(unregistration_called1);
  ASSERT_TRUE(unregistration_called2);
}

TEST_F(ServiceWorkerJobTest, AbortAll_RegUnreg) {
  GURL pattern("http://www.example.com/*");
  GURL script_url("http://www.example.com/service_worker.js");

  bool registration_called = false;
  scoped_refptr<ServiceWorkerRegistration> registration;
  job_coordinator()->Register(
      pattern,
      script_url,
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_ERROR_ABORT,
                       &registration_called, &registration));

  bool unregistration_called = false;
  job_coordinator()->Unregister(
      pattern,
      SaveUnregistration(SERVICE_WORKER_ERROR_ABORT,
                         &unregistration_called));

  ASSERT_FALSE(registration_called);
  ASSERT_FALSE(unregistration_called);
  job_coordinator()->AbortAll();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(registration_called);
  ASSERT_TRUE(unregistration_called);

  bool find_called = false;
  storage()->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          SERVICE_WORKER_ERROR_NOT_FOUND, &find_called, &registration));

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(find_called);
  EXPECT_EQ(scoped_refptr<ServiceWorkerRegistration>(), registration);
}

// Tests that the waiting worker enters the 'redundant' state upon
// unregistration.
TEST_F(ServiceWorkerJobTest, UnregisterWaitingSetsRedundant) {
  scoped_refptr<ServiceWorkerRegistration> registration;
  bool called = false;
  job_coordinator()->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &registration));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  ASSERT_TRUE(registration);

  // Manually create the waiting worker since there is no way to become a
  // waiting worker until Update is implemented.
  scoped_refptr<ServiceWorkerVersion> version = new ServiceWorkerVersion(
      registration, 1L, helper_->context()->AsWeakPtr());
  ServiceWorkerStatusCode status = SERVICE_WORKER_ERROR_FAILED;
  version->StartWorker(CreateReceiverOnCurrentThread(&status));
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(SERVICE_WORKER_OK, status);

  version->SetStatus(ServiceWorkerVersion::INSTALLED);
  registration->SetWaitingVersion(version);
  EXPECT_EQ(ServiceWorkerVersion::RUNNING,
            version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED, version->status());

  called = false;
  job_coordinator()->Unregister(GURL("http://www.example.com/*"),
                                SaveUnregistration(SERVICE_WORKER_OK, &called));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  // The version should be stopped since there is no controllee after
  // unregistration.
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version->status());
}

// Tests that the active worker enters the 'redundant' state upon
// unregistration.
TEST_F(ServiceWorkerJobTest, UnregisterActiveSetsRedundant) {
  scoped_refptr<ServiceWorkerRegistration> registration;
  bool called = false;
  job_coordinator()->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &registration));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  ASSERT_TRUE(registration);

  scoped_refptr<ServiceWorkerVersion> version = registration->active_version();
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, version->status());

  called = false;
  job_coordinator()->Unregister(GURL("http://www.example.com/*"),
                                SaveUnregistration(SERVICE_WORKER_OK, &called));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  // The version should be stopped since there is no controllee after
  // unregistration.
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version->status());
}

// Tests that the active worker enters the 'redundant' state upon
// unregistration.
TEST_F(ServiceWorkerJobTest,
       UnregisterActiveSetsRedundant_WaitForNoControllee) {
  scoped_refptr<ServiceWorkerRegistration> registration;
  bool called = false;
  job_coordinator()->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      render_process_id_,
      SaveRegistration(SERVICE_WORKER_OK, &called, &registration));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);
  ASSERT_TRUE(registration);

  scoped_ptr<ServiceWorkerProviderHost> host(
      new ServiceWorkerProviderHost(33 /* dummy render process id */,
                                    1 /* dummy provider_id */,
                                    context()->AsWeakPtr(),
                                    NULL));
  registration->active_version()->AddControllee(host.get());

  scoped_refptr<ServiceWorkerVersion> version = registration->active_version();
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, version->status());

  called = false;
  job_coordinator()->Unregister(GURL("http://www.example.com/*"),
                                SaveUnregistration(SERVICE_WORKER_OK, &called));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  // The version should be running since there is still a controllee.
  EXPECT_EQ(ServiceWorkerVersion::RUNNING, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED, version->status());

  registration->active_version()->RemoveControllee(host.get());
  base::RunLoop().RunUntilIdle();

  // The version should be stopped since there is no controllee.
  EXPECT_EQ(ServiceWorkerVersion::STOPPED, version->running_status());
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT, version->status());
}

namespace {  // Helpers for the update job tests.

const GURL kNoChangeOrigin("http://nochange/");
const GURL kNewVersionOrigin("http://newversion/");
const std::string kScope("scope/*");
const std::string kScript("script.js");

void RunNestedUntilIdle() {
  base::MessageLoop::ScopedNestableTaskAllower allow(
      base::MessageLoop::current());
  base::MessageLoop::current()->RunUntilIdle();
}

void OnIOComplete(int* rv_out, int rv) {
  *rv_out = rv;
}

void WriteResponse(
    ServiceWorkerStorage* storage, int64 id,
    const std::string& headers,
    IOBuffer* body, int length) {
  scoped_ptr<ServiceWorkerResponseWriter> writer =
      storage->CreateResponseWriter(id);

  scoped_ptr<net::HttpResponseInfo> info(new net::HttpResponseInfo);
  info->request_time = base::Time::Now();
  info->response_time = base::Time::Now();
  info->was_cached = false;
  info->headers = new net::HttpResponseHeaders(headers);
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
      new HttpResponseInfoIOBuffer(info.release());

  int rv = -1234;
  writer->WriteInfo(info_buffer, base::Bind(&OnIOComplete, &rv));
  RunNestedUntilIdle();
  EXPECT_LT(0, rv);

  rv = -1234;
  writer->WriteData(body, length,
                    base::Bind(&OnIOComplete, &rv));
  RunNestedUntilIdle();
  EXPECT_EQ(length, rv);
}

void WriteStringResponse(
    ServiceWorkerStorage* storage, int64 id,
    const std::string& body) {
  scoped_refptr<IOBuffer> body_buffer(new WrappedIOBuffer(body.data()));
  const char kHttpHeaders[] = "HTTP/1.0 200 HONKYDORY\0\0";
  std::string headers(kHttpHeaders, arraysize(kHttpHeaders));
  WriteResponse(storage, id, headers, body_buffer, body.length());
}

class UpdateJobTestHelper
    : public EmbeddedWorkerTestHelper,
      public ServiceWorkerRegistration::Listener,
      public ServiceWorkerVersion::Listener {
 public:
  struct AttributeChangeLogEntry {
    int64 registration_id;
    ChangedVersionAttributesMask mask;
    ServiceWorkerRegistrationInfo info;
  };

  struct StateChangeLogEntry {
    int64 version_id;
    ServiceWorkerVersion::Status status;
  };

  UpdateJobTestHelper(int mock_render_process_id)
      : EmbeddedWorkerTestHelper(mock_render_process_id) {}

  ServiceWorkerStorage* storage() { return context()->storage(); }
  ServiceWorkerJobCoordinator* job_coordinator() {
    return context()->job_coordinator();
  }

  scoped_refptr<ServiceWorkerRegistration> SetupInitialRegistration(
      const GURL& test_origin) {
    scoped_refptr<ServiceWorkerRegistration> registration;
    bool called = false;
    job_coordinator()->Register(
        test_origin.Resolve(kScope),
        test_origin.Resolve(kScript),
        mock_render_process_id(),
        SaveRegistration(SERVICE_WORKER_OK, &called, &registration));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(called);
    EXPECT_TRUE(registration);
    EXPECT_TRUE(registration->active_version());
    EXPECT_FALSE(registration->installing_version());
    EXPECT_FALSE(registration->waiting_version());
    return registration;
  }

  // EmbeddedWorkerTestHelper overrides
  virtual void OnStartWorker(int embedded_worker_id,
                             int64 version_id,
                             const GURL& scope,
                             const GURL& script,
                             bool pause_after_download) OVERRIDE {
    const std::string kMockScriptBody = "mock_script";
    ServiceWorkerVersion* version = context()->GetLiveVersion(version_id);
    ASSERT_TRUE(version);
    version->AddListener(this);

    if (!pause_after_download) {
      // Spoof caching the script for the initial version.
      int64 resource_id = storage()->NewResourceId();
      version->script_cache_map()->NotifyStartedCaching(script, resource_id);
      WriteStringResponse(storage(), resource_id, kMockScriptBody);
      version->script_cache_map()->NotifyFinishedCaching(script, true);
    } else {
      // Spoof caching the script for the new version.
      int64 resource_id = storage()->NewResourceId();
      version->script_cache_map()->NotifyStartedCaching(script, resource_id);
      if (script.GetOrigin() == kNoChangeOrigin)
        WriteStringResponse(storage(), resource_id, kMockScriptBody);
      else
        WriteStringResponse(storage(), resource_id, "mock_different_script");
      version->script_cache_map()->NotifyFinishedCaching(script, true);
    }
    EmbeddedWorkerTestHelper::OnStartWorker(
        embedded_worker_id, version_id, scope, script, pause_after_download);
  }

  // ServiceWorkerRegistration::Listener overrides
  virtual void OnVersionAttributesChanged(
      ServiceWorkerRegistration* registration,
      ChangedVersionAttributesMask changed_mask,
      const ServiceWorkerRegistrationInfo& info) OVERRIDE {
    AttributeChangeLogEntry entry;
    entry.registration_id = registration->id();
    entry.mask = changed_mask;
    entry.info = info;
    attribute_change_log_.push_back(entry);
  }

  // ServiceWorkerVersion::Listener overrides
  virtual void OnVersionStateChanged(ServiceWorkerVersion* version) OVERRIDE {
    StateChangeLogEntry entry;
    entry.version_id = version->version_id();
    entry.status = version->status();
    state_change_log_.push_back(entry);
  }

  std::vector<AttributeChangeLogEntry> attribute_change_log_;
  std::vector<StateChangeLogEntry> state_change_log_;
};

}  // namespace

TEST_F(ServiceWorkerJobTest, Update_NoChange) {
  UpdateJobTestHelper* update_helper =
      new UpdateJobTestHelper(render_process_id_);
  helper_.reset(update_helper);
  scoped_refptr<ServiceWorkerRegistration> registration =
      update_helper->SetupInitialRegistration(kNoChangeOrigin);
  ASSERT_TRUE(registration);
  ASSERT_EQ(4u, update_helper->state_change_log_.size());
  EXPECT_EQ(ServiceWorkerVersion::INSTALLING,
            update_helper->state_change_log_[0].status);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED,
            update_helper->state_change_log_[1].status);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING,
            update_helper->state_change_log_[2].status);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED,
            update_helper->state_change_log_[3].status);
  update_helper->state_change_log_.clear();

  // Run the update job.
  registration->AddListener(update_helper);
  scoped_refptr<ServiceWorkerVersion> first_version =
      registration->active_version();
  first_version->StartUpdate();
  base::RunLoop().RunUntilIdle();

  // Verify results.
  ASSERT_TRUE(registration->active_version());
  EXPECT_EQ(first_version.get(), registration->active_version());
  EXPECT_FALSE(registration->installing_version());
  EXPECT_FALSE(registration->waiting_version());
  EXPECT_TRUE(update_helper->attribute_change_log_.empty());
  ASSERT_EQ(1u, update_helper->state_change_log_.size());
  EXPECT_NE(registration->active_version()->version_id(),
            update_helper->state_change_log_[0].version_id);
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT,
            update_helper->state_change_log_[0].status);
}

TEST_F(ServiceWorkerJobTest, Update_NewVersion) {
  UpdateJobTestHelper* update_helper =
      new UpdateJobTestHelper(render_process_id_);
  helper_.reset(update_helper);
  scoped_refptr<ServiceWorkerRegistration> registration =
      update_helper->SetupInitialRegistration(kNewVersionOrigin);
  ASSERT_TRUE(registration);
  update_helper->state_change_log_.clear();

  // Run the update job.
  registration->AddListener(update_helper);
  scoped_refptr<ServiceWorkerVersion> first_version =
      registration->active_version();
  first_version->StartUpdate();
  base::RunLoop().RunUntilIdle();

  // Verify results.
  ASSERT_TRUE(registration->active_version());
  EXPECT_NE(first_version.get(), registration->active_version());
  EXPECT_FALSE(registration->installing_version());
  EXPECT_FALSE(registration->waiting_version());
  ASSERT_EQ(3u, update_helper->attribute_change_log_.size());

  UpdateJobTestHelper::AttributeChangeLogEntry entry;
  entry = update_helper->attribute_change_log_[0];
  EXPECT_TRUE(entry.mask.installing_changed());
  EXPECT_FALSE(entry.mask.waiting_changed());
  EXPECT_FALSE(entry.mask.active_changed());
  EXPECT_FALSE(entry.info.installing_version.is_null);
  EXPECT_TRUE(entry.info.waiting_version.is_null);
  EXPECT_FALSE(entry.info.active_version.is_null);

  entry = update_helper->attribute_change_log_[1];
  EXPECT_TRUE(entry.mask.installing_changed());
  EXPECT_TRUE(entry.mask.waiting_changed());
  EXPECT_FALSE(entry.mask.active_changed());
  EXPECT_TRUE(entry.info.installing_version.is_null);
  EXPECT_FALSE(entry.info.waiting_version.is_null);
  EXPECT_FALSE(entry.info.active_version.is_null);

  entry = update_helper->attribute_change_log_[2];
  EXPECT_FALSE(entry.mask.installing_changed());
  EXPECT_TRUE(entry.mask.waiting_changed());
  EXPECT_TRUE(entry.mask.active_changed());
  EXPECT_TRUE(entry.info.installing_version.is_null);
  EXPECT_TRUE(entry.info.waiting_version.is_null);
  EXPECT_FALSE(entry.info.active_version.is_null);

  // expected version state transitions:
  // new.installing, new.installed,
  // old.redundant,
  // new.activating, new.activated
  ASSERT_EQ(5u, update_helper->state_change_log_.size());

  EXPECT_EQ(registration->active_version()->version_id(),
            update_helper->state_change_log_[0].version_id);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLING,
            update_helper->state_change_log_[0].status);

  EXPECT_EQ(registration->active_version()->version_id(),
            update_helper->state_change_log_[1].version_id);
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED,
            update_helper->state_change_log_[1].status);

  EXPECT_EQ(first_version->version_id(),
            update_helper->state_change_log_[2].version_id);
  EXPECT_EQ(ServiceWorkerVersion::REDUNDANT,
            update_helper->state_change_log_[2].status);

  EXPECT_EQ(registration->active_version()->version_id(),
            update_helper->state_change_log_[3].version_id);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATING,
            update_helper->state_change_log_[3].status);

  EXPECT_EQ(registration->active_version()->version_id(),
            update_helper->state_change_log_[4].version_id);
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED,
            update_helper->state_change_log_[4].status);
}

}  // namespace content
