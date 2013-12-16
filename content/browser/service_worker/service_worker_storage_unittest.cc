// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_storage.h"

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

void SaveRegistrationCallback(
    ServiceWorkerRegistrationStatus expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration,
    ServiceWorkerRegistrationStatus status,
    const scoped_refptr<ServiceWorkerRegistration>& result) {
  EXPECT_EQ(expected_status, status);
  *called = true;
  *registration = result;
}

void SaveFoundRegistrationCallback(
    bool expected_found,
    ServiceWorkerRegistrationStatus expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration,
    bool found,
    ServiceWorkerRegistrationStatus status,
    const scoped_refptr<ServiceWorkerRegistration>& result) {
  EXPECT_EQ(expected_found, found);
  EXPECT_EQ(expected_status, status);
  *called = true;
  *registration = result;
}

// Creates a callback which both keeps track of if it's been called,
// as well as the resulting registration. Whent the callback is fired,
// it ensures that the resulting status matches the expectation.
// 'called' is useful for making sure a sychronous callback is or
// isn't called.
ServiceWorkerStorage::RegistrationCallback SaveRegistration(
    ServiceWorkerRegistrationStatus expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration) {
  *called = false;
  return base::Bind(
      &SaveRegistrationCallback, expected_status, called, registration);
}

ServiceWorkerStorage::FindRegistrationCallback SaveFoundRegistration(
    bool expected_found,
    ServiceWorkerRegistrationStatus expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration) {
  *called = false;
  return base::Bind(&SaveFoundRegistrationCallback,
                    expected_found,
                    expected_status,
                    called,
                    registration);
}

void SaveUnregistrationCallback(ServiceWorkerRegistrationStatus expected_status,
                                bool* called,
                                ServiceWorkerRegistrationStatus status) {
  EXPECT_EQ(expected_status, status);
  *called = true;
}

ServiceWorkerStorage::UnregistrationCallback SaveUnregistration(
    ServiceWorkerRegistrationStatus expected_status,
    bool* called) {
  *called = false;
  return base::Bind(&SaveUnregistrationCallback, expected_status, called);
}

}  // namespace

class ServiceWorkerStorageTest : public testing::Test {
 public:
  ServiceWorkerStorageTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() OVERRIDE {
    storage_.reset(new ServiceWorkerStorage(base::FilePath(), NULL));
  }

  virtual void TearDown() OVERRIDE { storage_.reset(); }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<ServiceWorkerStorage> storage_;
};

TEST_F(ServiceWorkerStorageTest, PatternMatches) {
  ASSERT_TRUE(ServiceWorkerStorage::PatternMatches(
      GURL("http://www.example.com/*"), GURL("http://www.example.com/")));
  ASSERT_TRUE(ServiceWorkerStorage::PatternMatches(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/page.html")));

  ASSERT_FALSE(ServiceWorkerStorage::PatternMatches(
      GURL("http://www.example.com/*"), GURL("https://www.example.com/")));
  ASSERT_FALSE(ServiceWorkerStorage::PatternMatches(
      GURL("http://www.example.com/*"),
      GURL("https://www.example.com/page.html")));

  ASSERT_FALSE(ServiceWorkerStorage::PatternMatches(
      GURL("http://www.example.com/*"), GURL("http://www.foo.com/")));
  ASSERT_FALSE(ServiceWorkerStorage::PatternMatches(
      GURL("http://www.example.com/*"), GURL("https://www.foo.com/page.html")));
}

TEST_F(ServiceWorkerStorageTest, SameDocumentSameRegistration) {
  scoped_refptr<ServiceWorkerRegistration> original_registration;
  bool called;
  storage_->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      SaveRegistration(REGISTRATION_OK, &called, &original_registration));
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> registration1;
  storage_->FindRegistrationForDocument(
      GURL("http://www.example.com/"),
      SaveFoundRegistration(true, REGISTRATION_OK, &called, &registration1));
  scoped_refptr<ServiceWorkerRegistration> registration2;
  storage_->FindRegistrationForDocument(
      GURL("http://www.example.com/"),
      SaveFoundRegistration(true, REGISTRATION_OK, &called, &registration2));

  ServiceWorkerRegistration* null_registration(NULL);
  ASSERT_EQ(null_registration, registration1);
  ASSERT_EQ(null_registration, registration2);
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  ASSERT_NE(null_registration, registration1);
  ASSERT_NE(null_registration, registration2);

  ASSERT_EQ(registration1, registration2);
}

TEST_F(ServiceWorkerStorageTest, SameMatchSameRegistration) {
  bool called;
  scoped_refptr<ServiceWorkerRegistration> original_registration;
  storage_->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      SaveRegistration(REGISTRATION_OK, &called, &original_registration));
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  ASSERT_NE(static_cast<ServiceWorkerRegistration*>(NULL),
            original_registration.get());

  scoped_refptr<ServiceWorkerRegistration> registration1;
  storage_->FindRegistrationForDocument(
      GURL("http://www.example.com/one"),
      SaveFoundRegistration(true, REGISTRATION_OK, &called, &registration1));

  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> registration2;
  storage_->FindRegistrationForDocument(
      GURL("http://www.example.com/two"),
      SaveFoundRegistration(true, REGISTRATION_OK, &called, &registration2));
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  ASSERT_EQ(registration1, registration2);
}

TEST_F(ServiceWorkerStorageTest, DifferentMatchDifferentRegistration) {
  bool called1;
  scoped_refptr<ServiceWorkerRegistration> original_registration1;
  storage_->Register(
      GURL("http://www.example.com/one/*"),
      GURL("http://www.example.com/service_worker.js"),
      SaveRegistration(REGISTRATION_OK, &called1, &original_registration1));

  bool called2;
  scoped_refptr<ServiceWorkerRegistration> original_registration2;
  storage_->Register(
      GURL("http://www.example.com/two/*"),
      GURL("http://www.example.com/service_worker.js"),
      SaveRegistration(REGISTRATION_OK, &called2, &original_registration2));

  EXPECT_FALSE(called1);
  EXPECT_FALSE(called2);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called2);
  EXPECT_TRUE(called1);

  scoped_refptr<ServiceWorkerRegistration> registration1;
  storage_->FindRegistrationForDocument(
      GURL("http://www.example.com/one/"),
      SaveFoundRegistration(true, REGISTRATION_OK, &called1, &registration1));
  scoped_refptr<ServiceWorkerRegistration> registration2;
  storage_->FindRegistrationForDocument(
      GURL("http://www.example.com/two/"),
      SaveFoundRegistration(true, REGISTRATION_OK, &called2, &registration2));

  EXPECT_FALSE(called1);
  EXPECT_FALSE(called2);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called2);
  EXPECT_TRUE(called1);

  ASSERT_NE(registration1, registration2);
}

// Make sure basic registration is working.
TEST_F(ServiceWorkerStorageTest, Register) {
  bool called = false;
  scoped_refptr<ServiceWorkerRegistration> registration;
  storage_->Register(GURL("http://www.example.com/*"),
                     GURL("http://www.example.com/service_worker.js"),
                     SaveRegistration(REGISTRATION_OK, &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_NE(scoped_refptr<ServiceWorkerRegistration>(NULL), registration);
}

// Make sure registrations are cleaned up when they are unregistered.
TEST_F(ServiceWorkerStorageTest, Unregister) {
  GURL pattern("http://www.example.com/*");

  bool called;
  scoped_refptr<ServiceWorkerRegistration> registration;
  storage_->Register(pattern,
                     GURL("http://www.example.com/service_worker.js"),
                     SaveRegistration(REGISTRATION_OK, &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  storage_->Unregister(pattern, SaveUnregistration(REGISTRATION_OK, &called));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_TRUE(registration->HasOneRef());

  storage_->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(false, REGISTRATION_OK, &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(scoped_refptr<ServiceWorkerRegistration>(NULL), registration);
}

// Make sure that when a new registration replaces an existing
// registration, that the old one is cleaned up.
TEST_F(ServiceWorkerStorageTest, RegisterNewScript) {
  GURL pattern("http://www.example.com/*");

  bool called;
  scoped_refptr<ServiceWorkerRegistration> old_registration;
  storage_->Register(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      SaveRegistration(REGISTRATION_OK, &called, &old_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> old_registration_by_pattern;
  storage_->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          true, REGISTRATION_OK, &called, &old_registration_by_pattern));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(old_registration, old_registration_by_pattern);
  old_registration_by_pattern = NULL;

  scoped_refptr<ServiceWorkerRegistration> new_registration;
  storage_->Register(
      pattern,
      GURL("http://www.example.com/service_worker_new.js"),
      SaveRegistration(REGISTRATION_OK, &called, &new_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_TRUE(old_registration->HasOneRef());

  ASSERT_NE(old_registration, new_registration);

  scoped_refptr<ServiceWorkerRegistration> new_registration_by_pattern;
  storage_->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(true, REGISTRATION_OK, &called, &new_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_NE(new_registration_by_pattern, old_registration);
}

// Make sure that when registering a duplicate pattern+script_url
// combination, that the same registration is used.
TEST_F(ServiceWorkerStorageTest, RegisterDuplicateScript) {
  GURL pattern("http://www.example.com/*");
  GURL script_url("http://www.example.com/service_worker.js");

  bool called;
  scoped_refptr<ServiceWorkerRegistration> old_registration;
  storage_->Register(
      pattern,
      script_url,
      SaveRegistration(REGISTRATION_OK, &called, &old_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> old_registration_by_pattern;
  storage_->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          true, REGISTRATION_OK, &called, &old_registration_by_pattern));
  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_TRUE(old_registration_by_pattern);

  scoped_refptr<ServiceWorkerRegistration> new_registration;
  storage_->Register(
      pattern,
      script_url,
      SaveRegistration(REGISTRATION_OK, &called, &new_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(old_registration, new_registration);

  ASSERT_FALSE(old_registration->HasOneRef());

  scoped_refptr<ServiceWorkerRegistration> new_registration_by_pattern;
  storage_->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          true, REGISTRATION_OK, &called, &new_registration_by_pattern));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(new_registration, old_registration);
}

}  // namespace content
