// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_disk_cache.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::IOBuffer;
using net::TestCompletionCallback;
using net::WrappedIOBuffer;

namespace content {

namespace {

typedef ServiceWorkerDatabase::RegistrationData RegistrationData;
typedef ServiceWorkerDatabase::ResourceRecord ResourceRecord;

void StatusCallback(bool* was_called,
                    ServiceWorkerStatusCode* result,
                    ServiceWorkerStatusCode status) {
  *was_called = true;
  *result = status;
}

ServiceWorkerStorage::StatusCallback MakeStatusCallback(
    bool* was_called,
    ServiceWorkerStatusCode* result) {
  return base::Bind(&StatusCallback, was_called, result);
}

void FindCallback(
    bool* was_called,
    ServiceWorkerStatusCode* result,
    scoped_refptr<ServiceWorkerRegistration>* found,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  *was_called = true;
  *result = status;
  *found = registration;
}

ServiceWorkerStorage::FindRegistrationCallback MakeFindCallback(
    bool* was_called,
    ServiceWorkerStatusCode* result,
    scoped_refptr<ServiceWorkerRegistration>* found) {
  return base::Bind(&FindCallback, was_called, result, found);
}

void GetAllCallback(
    bool* was_called,
    std::vector<ServiceWorkerRegistrationInfo>* all_out,
    const std::vector<ServiceWorkerRegistrationInfo>& all) {
  *was_called = true;
  *all_out = all;
}

ServiceWorkerStorage::GetAllRegistrationInfosCallback MakeGetAllCallback(
    bool* was_called,
    std::vector<ServiceWorkerRegistrationInfo>* all) {
  return base::Bind(&GetAllCallback, was_called, all);
}

void OnIOComplete(int* rv_out, int rv) {
  *rv_out = rv;
}

void OnCompareComplete(
    ServiceWorkerStatusCode* status_out, bool* are_equal_out,
    ServiceWorkerStatusCode status, bool are_equal) {
  *status_out = status;
  *are_equal_out = are_equal;
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
  base::RunLoop().RunUntilIdle();
  EXPECT_LT(0, rv);

  rv = -1234;
  writer->WriteData(body, length, base::Bind(&OnIOComplete, &rv));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(length, rv);
}

void WriteStringResponse(
    ServiceWorkerStorage* storage, int64 id,
    const std::string& headers,
    const std::string& body) {
  scoped_refptr<IOBuffer> body_buffer(new WrappedIOBuffer(body.data()));
  WriteResponse(storage, id, headers, body_buffer, body.length());
}

void WriteBasicResponse(ServiceWorkerStorage* storage, int64 id) {
  scoped_ptr<ServiceWorkerResponseWriter> writer =
      storage->CreateResponseWriter(id);

  const char kHttpHeaders[] = "HTTP/1.0 200 HONKYDORY\0Content-Length: 5\0\0";
  const char kHttpBody[] = "Hello";
  std::string headers(kHttpHeaders, arraysize(kHttpHeaders));
  WriteStringResponse(storage, id, headers, std::string(kHttpBody));
}

bool VerifyBasicResponse(ServiceWorkerStorage* storage, int64 id,
                         bool expected_positive_result) {
  const std::string kExpectedHttpBody("Hello");
  scoped_ptr<ServiceWorkerResponseReader> reader =
      storage->CreateResponseReader(id);
  scoped_refptr<HttpResponseInfoIOBuffer> info_buffer =
      new HttpResponseInfoIOBuffer();
  {
    TestCompletionCallback cb;
    reader->ReadInfo(info_buffer, cb.callback());
    int rv = cb.WaitForResult();
    if (expected_positive_result)
      EXPECT_LT(0, rv);
    if (rv <= 0)
      return false;
  }

  std::string received_body;
  {
    const int kBigEnough = 512;
    scoped_refptr<net::IOBuffer> buffer = new IOBuffer(kBigEnough);
    TestCompletionCallback cb;
    reader->ReadData(buffer, kBigEnough, cb.callback());
    int rv = cb.WaitForResult();
    EXPECT_EQ(static_cast<int>(kExpectedHttpBody.size()), rv);
    if (rv <= 0)
      return false;
    received_body.assign(buffer->data(), rv);
  }

  bool status_match =
      std::string("HONKYDORY") ==
          info_buffer->http_info->headers->GetStatusText();
  bool data_match = kExpectedHttpBody == received_body;

  EXPECT_TRUE(status_match);
  EXPECT_TRUE(data_match);
  return status_match && data_match;
}

void WriteResponseOfSize(ServiceWorkerStorage* storage, int64 id,
                         char val, int size) {
  const char kHttpHeaders[] = "HTTP/1.0 200 HONKYDORY\00";
  std::string headers(kHttpHeaders, arraysize(kHttpHeaders));
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(size);
  memset(buffer->data(), val, size);
  WriteResponse(storage, id, headers, buffer, size);
}

}  // namespace

class ServiceWorkerStorageTest : public testing::Test {
 public:
  ServiceWorkerStorageTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {
  }

  virtual void SetUp() OVERRIDE {
    context_.reset(
        new ServiceWorkerContextCore(GetUserDataDirectory(),
                                     base::MessageLoopProxy::current(),
                                     base::MessageLoopProxy::current(),
                                     base::MessageLoopProxy::current(),
                                     NULL,
                                     NULL,
                                     NULL));
    context_ptr_ = context_->AsWeakPtr();
  }

  virtual void TearDown() OVERRIDE {
    context_.reset();
  }

  virtual base::FilePath GetUserDataDirectory() { return base::FilePath(); }

  ServiceWorkerStorage* storage() { return context_->storage(); }

  // A static class method for friendliness.
  static void VerifyPurgeableListStatusCallback(
      ServiceWorkerDatabase* database,
      std::set<int64> *purgeable_ids,
      bool* was_called,
      ServiceWorkerStatusCode* result,
      ServiceWorkerStatusCode status) {
    *was_called = true;
    *result = status;
    EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
              database->GetPurgeableResourceIds(purgeable_ids));
  }

 protected:
  ServiceWorkerStatusCode StoreRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration,
      scoped_refptr<ServiceWorkerVersion> version) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
    storage()->StoreRegistration(
        registration, version, MakeStatusCallback(&was_called, &result));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode DeleteRegistration(
      int64 registration_id,
      const GURL& origin) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
    storage()->DeleteRegistration(
        registration_id, origin, MakeStatusCallback(&was_called, &result));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  void GetAllRegistrations(
      std::vector<ServiceWorkerRegistrationInfo>* registrations) {
    bool was_called = false;
    storage()->GetAllRegistrations(
        MakeGetAllCallback(&was_called, registrations));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
  }

  ServiceWorkerStatusCode UpdateToActiveState(
      scoped_refptr<ServiceWorkerRegistration> registration) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
    storage()->UpdateToActiveState(
        registration, MakeStatusCallback(&was_called, &result));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode FindRegistrationForDocument(
      const GURL& document_url,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
    storage()->FindRegistrationForDocument(
        document_url, MakeFindCallback(&was_called, &result, registration));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode FindRegistrationForPattern(
      const GURL& scope,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
    storage()->FindRegistrationForPattern(
        scope, MakeFindCallback(&was_called, &result, registration));
    EXPECT_FALSE(was_called);  // always async
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  ServiceWorkerStatusCode FindRegistrationForId(
      int64 registration_id,
      const GURL& origin,
      scoped_refptr<ServiceWorkerRegistration>* registration) {
    bool was_called = false;
    ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
    storage()->FindRegistrationForId(
        registration_id, origin,
        MakeFindCallback(&was_called, &result, registration));
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(was_called);
    return result;
  }

  scoped_ptr<ServiceWorkerContextCore> context_;
  base::WeakPtr<ServiceWorkerContextCore> context_ptr_;
  TestBrowserThreadBundle browser_thread_bundle_;
};

TEST_F(ServiceWorkerStorageTest, StoreFindUpdateDeleteRegistration) {
  const GURL kScope("http://www.test.not/scope/");
  const GURL kScript("http://www.test.not/script.js");
  const GURL kDocumentUrl("http://www.test.not/scope/document.html");
  const int64 kRegistrationId = 0;
  const int64 kVersionId = 0;

  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // We shouldn't find anything without having stored anything.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_FALSE(found_registration);

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_FALSE(found_registration);

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_FALSE(found_registration);

  // Store something.
  scoped_refptr<ServiceWorkerRegistration> live_registration =
      new ServiceWorkerRegistration(
          kScope, kScript, kRegistrationId, context_ptr_);
  scoped_refptr<ServiceWorkerVersion> live_version =
      new ServiceWorkerVersion(
          live_registration, kVersionId, context_ptr_);
  live_version->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_registration->SetWaitingVersion(live_version);
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(live_registration, live_version));

  // Now we should find it and get the live ptr back immediately.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  // But FindRegistrationForPattern is always async.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  // Can be found by id too.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  ASSERT_TRUE(found_registration);
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  // Drop the live registration, but keep the version live.
  live_registration = NULL;

  // Now FindRegistrationForDocument should be async.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  ASSERT_TRUE(found_registration);
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());
  EXPECT_EQ(live_version, found_registration->waiting_version());
  found_registration = NULL;

  // Drop the live version too.
  live_version = NULL;

  // And FindRegistrationForPattern is always async.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForPattern(kScope, &found_registration));
  ASSERT_TRUE(found_registration);
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());
  EXPECT_FALSE(found_registration->active_version());
  ASSERT_TRUE(found_registration->waiting_version());
  EXPECT_EQ(ServiceWorkerVersion::INSTALLED,
            found_registration->waiting_version()->status());

  // Update to active.
  scoped_refptr<ServiceWorkerVersion> temp_version =
      found_registration->waiting_version();
  temp_version->SetStatus(ServiceWorkerVersion::ACTIVATED);
  found_registration->SetActiveVersion(temp_version);
  temp_version = NULL;
  EXPECT_EQ(SERVICE_WORKER_OK, UpdateToActiveState(found_registration));
  found_registration = NULL;

  // Trying to update a unstored registration to active should fail.
  scoped_refptr<ServiceWorkerRegistration> unstored_registration =
      new ServiceWorkerRegistration(
          kScope, kScript, kRegistrationId + 1, context_ptr_);
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            UpdateToActiveState(unstored_registration));
  unstored_registration = NULL;

  // The Find methods should return a registration with an active version.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  ASSERT_TRUE(found_registration);
  EXPECT_EQ(kRegistrationId, found_registration->id());
  EXPECT_TRUE(found_registration->HasOneRef());
  EXPECT_FALSE(found_registration->waiting_version());
  ASSERT_TRUE(found_registration->active_version());
  EXPECT_EQ(ServiceWorkerVersion::ACTIVATED,
            found_registration->active_version()->status());

  // Delete from storage but with a instance still live.
  EXPECT_TRUE(context_->GetLiveVersion(kRegistrationId));
  EXPECT_EQ(SERVICE_WORKER_OK,
            DeleteRegistration(kRegistrationId, kScope.GetOrigin()));
  EXPECT_TRUE(context_->GetLiveVersion(kRegistrationId));

  // Should no longer be found.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_FALSE(found_registration);

  // Deleting an unstored registration should succeed.
  EXPECT_EQ(SERVICE_WORKER_OK,
            DeleteRegistration(kRegistrationId + 1, kScope.GetOrigin()));
}

TEST_F(ServiceWorkerStorageTest, InstallingRegistrationsAreFindable) {
  const GURL kScope("http://www.test.not/scope/");
  const GURL kScript("http://www.test.not/script.js");
  const GURL kDocumentUrl("http://www.test.not/scope/document.html");
  const int64 kRegistrationId = 0;
  const int64 kVersionId = 0;

  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // Create an unstored registration.
  scoped_refptr<ServiceWorkerRegistration> live_registration =
      new ServiceWorkerRegistration(
          kScope, kScript, kRegistrationId, context_ptr_);
  scoped_refptr<ServiceWorkerVersion> live_version =
      new ServiceWorkerVersion(
          live_registration, kVersionId, context_ptr_);
  live_version->SetStatus(ServiceWorkerVersion::INSTALLING);
  live_registration->SetWaitingVersion(live_version);

  // Should not be findable, including by GetAllRegistrations.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_FALSE(found_registration);

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_FALSE(found_registration);

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_FALSE(found_registration);

  std::vector<ServiceWorkerRegistrationInfo> all_registrations;
  GetAllRegistrations(&all_registrations);
  EXPECT_TRUE(all_registrations.empty());

  // Notify storage of it being installed.
  storage()->NotifyInstallingRegistration(live_registration);

  // Now should be findable.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_EQ(live_registration, found_registration);
  found_registration = NULL;

  GetAllRegistrations(&all_registrations);
  EXPECT_EQ(1u, all_registrations.size());
  all_registrations.clear();

  // Notify storage of installation no longer happening.
  storage()->NotifyDoneInstallingRegistration(
      live_registration, NULL, SERVICE_WORKER_OK);

  // Once again, should not be findable.
  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForId(
                kRegistrationId, kScope.GetOrigin(), &found_registration));
  EXPECT_FALSE(found_registration);

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_FALSE(found_registration);

  EXPECT_EQ(SERVICE_WORKER_ERROR_NOT_FOUND,
            FindRegistrationForPattern(kScope, &found_registration));
  EXPECT_FALSE(found_registration);

  GetAllRegistrations(&all_registrations);
  EXPECT_TRUE(all_registrations.empty());
}

class ServiceWorkerResourceStorageTest : public ServiceWorkerStorageTest {
 public:
  virtual void SetUp() OVERRIDE {
    ServiceWorkerStorageTest::SetUp();

    storage()->LazyInitialize(base::Bind(&base::DoNothing));
    base::RunLoop().RunUntilIdle();
    scope_ = GURL("http://www.test.not/scope/");
    script_ = GURL("http://www.test.not/script.js");
    import_ = GURL("http://www.test.not/import.js");
    document_url_ = GURL("http://www.test.not/scope/document.html");
    registration_id_ = storage()->NewRegistrationId();
    version_id_ = storage()->NewVersionId();
    resource_id1_ = storage()->NewResourceId();
    resource_id2_ = storage()->NewResourceId();

    // Cons up a new registration+version with two script resources.
    RegistrationData data;
    data.registration_id = registration_id_;
    data.scope = scope_;
    data.script = script_;
    data.version_id = version_id_;
    data.is_active = false;
    std::vector<ResourceRecord> resources;
    resources.push_back(ResourceRecord(resource_id1_, script_));
    resources.push_back(ResourceRecord(resource_id2_, import_));
    registration_ = storage()->GetOrCreateRegistration(data, resources);
    registration_->waiting_version()->SetStatus(ServiceWorkerVersion::NEW);

    // Add the resources ids to the uncommitted list.
    storage()->StoreUncommittedResponseId(resource_id1_);
    storage()->StoreUncommittedResponseId(resource_id2_);
    base::RunLoop().RunUntilIdle();
    std::set<int64> verify_ids;
    EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
              storage()->database_->GetUncommittedResourceIds(&verify_ids));
    EXPECT_EQ(2u, verify_ids.size());

    // And dump something in the disk cache for them.
    WriteBasicResponse(storage(), resource_id1_);
    WriteBasicResponse(storage(), resource_id2_);
    EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, true));
    EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id2_, true));

    // Storing the registration/version should take the resources ids out
    // of the uncommitted list.
    EXPECT_EQ(
        SERVICE_WORKER_OK,
        StoreRegistration(registration_, registration_->waiting_version()));
    verify_ids.clear();
    EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
              storage()->database_->GetUncommittedResourceIds(&verify_ids));
    EXPECT_TRUE(verify_ids.empty());
  }

 protected:
  GURL scope_;
  GURL script_;
  GURL import_;
  GURL document_url_;
  int64 registration_id_;
  int64 version_id_;
  int64 resource_id1_;
  int64 resource_id2_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
};

class ServiceWorkerResourceStorageDiskTest
    : public ServiceWorkerResourceStorageTest {
 public:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(user_data_directory_.CreateUniqueTempDir());
    ServiceWorkerResourceStorageTest::SetUp();
  }

  virtual base::FilePath GetUserDataDirectory() OVERRIDE {
    return user_data_directory_.path();
  }

 protected:
  base::ScopedTempDir user_data_directory_;
};

TEST_F(ServiceWorkerResourceStorageTest, DeleteRegistration_NoLiveVersion) {
  bool was_called = false;
  ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
  std::set<int64> verify_ids;

  registration_->SetWaitingVersion(NULL);
  registration_ = NULL;

  // Deleting the registration should result in the resources being added to the
  // purgeable list and then doomed in the disk cache and removed from that
  // list.
  storage()->DeleteRegistration(
      registration_id_,
      scope_.GetOrigin(),
      base::Bind(&VerifyPurgeableListStatusCallback,
                 base::Unretained(storage()->database_.get()),
                 &verify_ids,
                 &was_called,
                 &result));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(was_called);
  EXPECT_EQ(SERVICE_WORKER_OK, result);
  EXPECT_EQ(2u, verify_ids.size());
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_TRUE(verify_ids.empty());

  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id2_, false));
}

TEST_F(ServiceWorkerResourceStorageTest, DeleteRegistration_WaitingVersion) {
  bool was_called = false;
  ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
  std::set<int64> verify_ids;

  // Deleting the registration should result in the resources being added to the
  // purgeable list and then doomed in the disk cache and removed from that
  // list.
  storage()->DeleteRegistration(
      registration_->id(),
      scope_.GetOrigin(),
      base::Bind(&VerifyPurgeableListStatusCallback,
                 base::Unretained(storage()->database_.get()),
                 &verify_ids,
                 &was_called,
                 &result));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(was_called);
  EXPECT_EQ(SERVICE_WORKER_OK, result);
  EXPECT_EQ(2u, verify_ids.size());
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_EQ(2u, verify_ids.size());

  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id2_, false));

  // Doom the version, now it happens.
  registration_->waiting_version()->Doom();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, result);
  EXPECT_EQ(2u, verify_ids.size());
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_TRUE(verify_ids.empty());

  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id2_, false));
}

TEST_F(ServiceWorkerResourceStorageTest, DeleteRegistration_ActiveVersion) {
  // Promote the worker to active and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  storage()->UpdateToActiveState(
      registration_, base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  scoped_ptr<ServiceWorkerProviderHost> host(
      new ServiceWorkerProviderHost(33 /* dummy render process id */,
                                    1 /* dummy provider_id */,
                                    context_->AsWeakPtr(),
                                    NULL));
  registration_->active_version()->AddControllee(host.get());

  bool was_called = false;
  ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
  std::set<int64> verify_ids;

  // Deleting the registration should move the resources to the purgeable list
  // but keep them available.
  storage()->DeleteRegistration(
      registration_->id(),
      scope_.GetOrigin(),
      base::Bind(&VerifyPurgeableListStatusCallback,
                 base::Unretained(storage()->database_.get()),
                 &verify_ids,
                 &was_called,
                 &result));
  registration_->active_version()->Doom();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(was_called);
  EXPECT_EQ(SERVICE_WORKER_OK, result);
  EXPECT_EQ(2u, verify_ids.size());
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_EQ(2u, verify_ids.size());

  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, true));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id2_, true));

  // Removing the controllee should cause the resources to be deleted.
  registration_->active_version()->RemoveControllee(host.get());
  base::RunLoop().RunUntilIdle();
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_TRUE(verify_ids.empty());

  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id2_, false));
}

// Android has flaky IO error: http://crbug.com/387045
#if defined(OS_ANDROID)
#define MAYBE_CleanupOnRestart DISABLED_CleanupOnRestart
#else
#define MAYBE_CleanupOnRestart CleanupOnRestart
#endif
TEST_F(ServiceWorkerResourceStorageDiskTest, MAYBE_CleanupOnRestart) {
  // Promote the worker to active and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  registration_->SetWaitingVersion(NULL);
  storage()->UpdateToActiveState(
      registration_, base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  scoped_ptr<ServiceWorkerProviderHost> host(
      new ServiceWorkerProviderHost(33 /* dummy render process id */,
                                    1 /* dummy provider_id */,
                                    context_->AsWeakPtr(),
                                    NULL));
  registration_->active_version()->AddControllee(host.get());

  bool was_called = false;
  ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
  std::set<int64> verify_ids;

  // Deleting the registration should move the resources to the purgeable list
  // but keep them available.
  storage()->DeleteRegistration(
      registration_->id(),
      scope_.GetOrigin(),
      base::Bind(&VerifyPurgeableListStatusCallback,
                 base::Unretained(storage()->database_.get()),
                 &verify_ids,
                 &was_called,
                 &result));
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(was_called);
  EXPECT_EQ(SERVICE_WORKER_OK, result);
  EXPECT_EQ(2u, verify_ids.size());
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_EQ(2u, verify_ids.size());

  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, true));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id2_, true));

  // Also add an uncommitted resource.
  int64 kStaleUncommittedResourceId = storage()->NewResourceId();
  storage()->StoreUncommittedResponseId(kStaleUncommittedResourceId);
  base::RunLoop().RunUntilIdle();
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetUncommittedResourceIds(&verify_ids));
  EXPECT_EQ(1u, verify_ids.size());
  WriteBasicResponse(storage(), kStaleUncommittedResourceId);
  EXPECT_TRUE(
      VerifyBasicResponse(storage(), kStaleUncommittedResourceId, true));

  // Simulate browser shutdown. The purgeable and uncommitted resources are now
  // stale.
  context_.reset();
  context_.reset(new ServiceWorkerContextCore(GetUserDataDirectory(),
                                              base::MessageLoopProxy::current(),
                                              base::MessageLoopProxy::current(),
                                              base::MessageLoopProxy::current(),
                                              NULL,
                                              NULL,
                                              NULL));
  storage()->LazyInitialize(base::Bind(&base::DoNothing));
  base::RunLoop().RunUntilIdle();

  // Store a new uncommitted resource. This triggers stale resource cleanup.
  int64 kNewResourceId = storage()->NewResourceId();
  WriteBasicResponse(storage(), kNewResourceId);
  storage()->StoreUncommittedResponseId(kNewResourceId);
  base::RunLoop().RunUntilIdle();

  // The stale resources should be purged, but the new resource should persist.
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetUncommittedResourceIds(&verify_ids));
  ASSERT_EQ(1u, verify_ids.size());
  EXPECT_EQ(kNewResourceId, *verify_ids.begin());

  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_TRUE(verify_ids.empty());
  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id2_, false));
  EXPECT_FALSE(
      VerifyBasicResponse(storage(), kStaleUncommittedResourceId, false));
  EXPECT_TRUE(VerifyBasicResponse(storage(), kNewResourceId, true));
}

TEST_F(ServiceWorkerResourceStorageTest, UpdateRegistration) {
  // Promote the worker to active worker and add a controllee.
  registration_->SetActiveVersion(registration_->waiting_version());
  storage()->UpdateToActiveState(
      registration_, base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
  scoped_ptr<ServiceWorkerProviderHost> host(
      new ServiceWorkerProviderHost(33 /* dummy render process id */,
                                    1 /* dummy provider_id */,
                                    context_->AsWeakPtr(),
                                    NULL));
  registration_->active_version()->AddControllee(host.get());

  bool was_called = false;
  ServiceWorkerStatusCode result = SERVICE_WORKER_ERROR_FAILED;
  std::set<int64> verify_ids;

  // Make an updated registration.
  scoped_refptr<ServiceWorkerVersion> live_version = new ServiceWorkerVersion(
      registration_, storage()->NewVersionId(), context_ptr_);
  live_version->SetStatus(ServiceWorkerVersion::NEW);
  registration_->SetWaitingVersion(live_version);

  // Writing the registration should move the old version's resources to the
  // purgeable list but keep them available.
  storage()->StoreRegistration(
      registration_,
      registration_->waiting_version(),
      base::Bind(&VerifyPurgeableListStatusCallback,
                 base::Unretained(storage()->database_.get()),
                 &verify_ids,
                 &was_called,
                 &result));
  registration_->active_version()->Doom();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(was_called);
  EXPECT_EQ(SERVICE_WORKER_OK, result);
  EXPECT_EQ(2u, verify_ids.size());
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_EQ(2u, verify_ids.size());

  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_TRUE(VerifyBasicResponse(storage(), resource_id2_, false));

  // Removing the controllee should cause the old version's resources to be
  // deleted.
  registration_->active_version()->RemoveControllee(host.get());
  base::RunLoop().RunUntilIdle();
  verify_ids.clear();
  EXPECT_EQ(ServiceWorkerDatabase::STATUS_OK,
            storage()->database_->GetPurgeableResourceIds(&verify_ids));
  EXPECT_TRUE(verify_ids.empty());

  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id1_, false));
  EXPECT_FALSE(VerifyBasicResponse(storage(), resource_id2_, false));
}

TEST_F(ServiceWorkerStorageTest, FindRegistration_LongestScopeMatch) {
  const GURL kDocumentUrl("http://www.example.com/scope/foo");
  scoped_refptr<ServiceWorkerRegistration> found_registration;

  // Registration for "/scope/".
  const GURL kScope1("http://www.example.com/scope/");
  const GURL kScript1("http://www.example.com/script1.js");
  const int64 kRegistrationId1 = 1;
  const int64 kVersionId1 = 1;
  scoped_refptr<ServiceWorkerRegistration> live_registration1 =
      new ServiceWorkerRegistration(
          kScope1, kScript1, kRegistrationId1, context_ptr_);
  scoped_refptr<ServiceWorkerVersion> live_version1 =
      new ServiceWorkerVersion(
          live_registration1, kVersionId1, context_ptr_);
  live_version1->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_registration1->SetWaitingVersion(live_version1);

  // Registration for "/scope/foo".
  const GURL kScope2("http://www.example.com/scope/foo");
  const GURL kScript2("http://www.example.com/script2.js");
  const int64 kRegistrationId2 = 2;
  const int64 kVersionId2 = 2;
  scoped_refptr<ServiceWorkerRegistration> live_registration2 =
      new ServiceWorkerRegistration(
          kScope2, kScript2, kRegistrationId2, context_ptr_);
  scoped_refptr<ServiceWorkerVersion> live_version2 =
      new ServiceWorkerVersion(
          live_registration2, kVersionId2, context_ptr_);
  live_version2->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_registration2->SetWaitingVersion(live_version2);

  // Registration for "/scope/foobar".
  const GURL kScope3("http://www.example.com/scope/foobar");
  const GURL kScript3("http://www.example.com/script3.js");
  const int64 kRegistrationId3 = 3;
  const int64 kVersionId3 = 3;
  scoped_refptr<ServiceWorkerRegistration> live_registration3 =
      new ServiceWorkerRegistration(
          kScope3, kScript3, kRegistrationId3, context_ptr_);
  scoped_refptr<ServiceWorkerVersion> live_version3 =
      new ServiceWorkerVersion(
          live_registration3, kVersionId3, context_ptr_);
  live_version3->SetStatus(ServiceWorkerVersion::INSTALLED);
  live_registration3->SetWaitingVersion(live_version3);

  // Notify storage of they being installed.
  storage()->NotifyInstallingRegistration(live_registration1);
  storage()->NotifyInstallingRegistration(live_registration2);
  storage()->NotifyInstallingRegistration(live_registration3);

  // Find a registration among installing ones.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration2, found_registration);
  found_registration = NULL;

  // Store registrations.
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(live_registration1, live_version1));
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(live_registration2, live_version2));
  EXPECT_EQ(SERVICE_WORKER_OK,
            StoreRegistration(live_registration3, live_version3));

  // Notify storage of installations no longer happening.
  storage()->NotifyDoneInstallingRegistration(
      live_registration1, NULL, SERVICE_WORKER_OK);
  storage()->NotifyDoneInstallingRegistration(
      live_registration2, NULL, SERVICE_WORKER_OK);
  storage()->NotifyDoneInstallingRegistration(
      live_registration3, NULL, SERVICE_WORKER_OK);

  // Find a registration among installed ones.
  EXPECT_EQ(SERVICE_WORKER_OK,
            FindRegistrationForDocument(kDocumentUrl, &found_registration));
  EXPECT_EQ(live_registration2, found_registration);
}

TEST_F(ServiceWorkerStorageTest, CompareResources) {
  // Compare two small responses containing the same data.
  WriteBasicResponse(storage(), 1);
  WriteBasicResponse(storage(), 2);
  ServiceWorkerStatusCode status = static_cast<ServiceWorkerStatusCode>(-1);
  bool are_equal = false;
  storage()->CompareScriptResources(
      1, 2,
      base::Bind(&OnCompareComplete, &status, &are_equal));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(are_equal);

  // Compare two small responses with different data.
  const char kHttpHeaders[] = "HTTP/1.0 200 HONKYDORY\0\0";
  const char kHttpBody[] = "Goodbye";
  std::string headers(kHttpHeaders, arraysize(kHttpHeaders));
  WriteStringResponse(storage(), 3, headers, std::string(kHttpBody));
  status = static_cast<ServiceWorkerStatusCode>(-1);
  are_equal = true;
  storage()->CompareScriptResources(
      1, 3,
      base::Bind(&OnCompareComplete, &status, &are_equal));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_FALSE(are_equal);

  // Compare two large responses with the same data.
  const int k32K = 32 * 1024;
  WriteResponseOfSize(storage(), 4, 'a', k32K);
  WriteResponseOfSize(storage(), 5, 'a', k32K);
  status = static_cast<ServiceWorkerStatusCode>(-1);
  are_equal = false;
  storage()->CompareScriptResources(
      4, 5,
      base::Bind(&OnCompareComplete, &status, &are_equal));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_TRUE(are_equal);

  // Compare a large and small response.
  status = static_cast<ServiceWorkerStatusCode>(-1);
  are_equal = true;
  storage()->CompareScriptResources(
      1, 5,
      base::Bind(&OnCompareComplete, &status, &are_equal));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_FALSE(are_equal);

  // Compare two large responses with different data.
  WriteResponseOfSize(storage(), 6, 'b', k32K);
  status = static_cast<ServiceWorkerStatusCode>(-1);
  are_equal = true;
  storage()->CompareScriptResources(
      5, 6,
      base::Bind(&OnCompareComplete, &status, &are_equal));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SERVICE_WORKER_OK, status);
  EXPECT_FALSE(are_equal);
}

}  // namespace content
