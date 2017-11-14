// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
namespace {

using ::testing::UnorderedElementsAre;
using ::testing::IsEmpty;

enum class BackgroundFetchRegistrationStorage { kPersistent, kNonPersistent };

const char kUserDataPrefix[] = "bgfetch_";

const char kExampleDeveloperId[] = "my-example-id";
const char kAlternativeDeveloperId[] = "my-other-id";
const char kExampleUniqueId[] = "7e57ab1e-c0de-a150-ca75-1e75f005ba11";
const char kAlternativeUniqueId[] = "bb48a9fb-c21f-4c2d-a9ae-58bd48a9fb53";

// See schema documentation in background_fetch_data_manager.cc.
// A "bgfetch_registration_" per registration (not including keys for requests).
constexpr size_t kUserDataKeysPerInactiveRegistration = 1u;

// A "bgfetch_request_" per request.
constexpr size_t kUserDataKeysPerInactiveRequest = 1u;

void DidCreateRegistration(
    base::Closure quit_closure,
    blink::mojom::BackgroundFetchError* out_error,
    blink::mojom::BackgroundFetchError error,
    std::unique_ptr<BackgroundFetchRegistration> registration) {
  *out_error = error;
  quit_closure.Run();
}

void DidGetError(base::Closure quit_closure,
                 blink::mojom::BackgroundFetchError* out_error,
                 blink::mojom::BackgroundFetchError error) {
  *out_error = error;
  quit_closure.Run();
}

void DidGetRegistrationUserDataByKeyPrefix(base::Closure quit_closure,
                                           std::vector<std::string>* out_data,
                                           const std::vector<std::string>& data,
                                           ServiceWorkerStatusCode status) {
  DCHECK(out_data);
  DCHECK_EQ(SERVICE_WORKER_OK, status);
  *out_data = data;
  quit_closure.Run();
}

}  // namespace

class BackgroundFetchDataManagerTest
    : public BackgroundFetchTestBase,
      public ::testing::WithParamInterface<BackgroundFetchRegistrationStorage> {
 public:
  BackgroundFetchDataManagerTest() {
    registration_storage_ = GetParam();
    if (registration_storage_ ==
        BackgroundFetchRegistrationStorage::kPersistent) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableBackgroundFetchPersistence);
    }
    RestartDataManagerFromPersistentStorage();
  }

  ~BackgroundFetchDataManagerTest() override = default;

  // Re-creates the data manager. Useful for testing that data was persisted.
  // If the test is non-persistent mode (e.g. testing the old code path), then
  // this does nothing after the first call.
  void RestartDataManagerFromPersistentStorage() {
    if (registration_storage_ ==
            BackgroundFetchRegistrationStorage::kNonPersistent &&
        background_fetch_data_manager_) {
      return;
    }

    background_fetch_data_manager_ =
        std::make_unique<BackgroundFetchDataManager>(
            browser_context(),
            embedded_worker_test_helper()->context_wrapper());
  }

  // Synchronous version of BackgroundFetchDataManager::CreateRegistration().
  void CreateRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      const std::vector<ServiceWorkerFetchRequest>& requests,
      const BackgroundFetchOptions& options,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    background_fetch_data_manager_->CreateRegistration(
        registration_id, requests, options,
        base::BindOnce(&DidCreateRegistration, run_loop.QuitClosure(),
                       out_error));
    run_loop.Run();
  }

  std::unique_ptr<BackgroundFetchRegistration> GetRegistration(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      const std::string developer_id,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    std::unique_ptr<BackgroundFetchRegistration> registration;
    base::RunLoop run_loop;
    background_fetch_data_manager_->GetRegistration(
        service_worker_registration_id, origin, developer_id,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidGetRegistration,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, &registration));
    run_loop.Run();

    return registration;
  }

  std::vector<std::string> GetDeveloperIds(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    std::vector<std::string> ids;
    base::RunLoop run_loop;
    background_fetch_data_manager_->GetDeveloperIdsForServiceWorker(
        service_worker_registration_id, origin,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidGetDeveloperIds,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, &ids));
    run_loop.Run();

    return ids;
  }

  // Synchronous version of
  // BackgroundFetchDataManager::MarkRegistrationForDeletion().
  void MarkRegistrationForDeletion(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    background_fetch_data_manager_->MarkRegistrationForDeletion(
        registration_id,
        base::BindOnce(&DidGetError, run_loop.QuitClosure(), out_error));
    run_loop.Run();
  }

  // Synchronous version of BackgroundFetchDataManager::DeleteRegistration().
  void DeleteRegistration(const BackgroundFetchRegistrationId& registration_id,
                          blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    background_fetch_data_manager_->DeleteRegistration(
        registration_id,
        base::BindOnce(&DidGetError, run_loop.QuitClosure(), out_error));
    run_loop.Run();
  }

  // Synchronous version of
  // ServiceWorkerContextWrapper::GetRegistrationUserDataByKeyPrefix.
  std::vector<std::string> GetRegistrationUserDataByKeyPrefix(
      int64_t service_worker_registration_id,
      const std::string& key_prefix) {
    std::vector<std::string> data;

    base::RunLoop run_loop;
    embedded_worker_test_helper()
        ->context_wrapper()
        ->GetRegistrationUserDataByKeyPrefix(
            service_worker_registration_id, key_prefix,
            base::Bind(&DidGetRegistrationUserDataByKeyPrefix,
                       run_loop.QuitClosure(), &data));
    run_loop.Run();

    return data;
  }

 protected:
  void DidGetRegistration(
      base::Closure quit_closure,
      blink::mojom::BackgroundFetchError* out_error,
      std::unique_ptr<BackgroundFetchRegistration>* out_registration,
      blink::mojom::BackgroundFetchError error,
      std::unique_ptr<BackgroundFetchRegistration> registration) {
    if (error == blink::mojom::BackgroundFetchError::NONE) {
      DCHECK(registration);
    }
    *out_error = error;
    *out_registration = std::move(registration);

    quit_closure.Run();
  }

  void DidGetDeveloperIds(base::Closure quit_closure,
                          blink::mojom::BackgroundFetchError* out_error,
                          std::vector<std::string>* out_ids,
                          blink::mojom::BackgroundFetchError error,
                          const std::vector<std::string>& ids) {
    *out_error = error;
    *out_ids = ids;

    quit_closure.Run();
  }

  BackgroundFetchRegistrationStorage registration_storage_;
  std::unique_ptr<BackgroundFetchDataManager> background_fetch_data_manager_;
};

INSTANTIATE_TEST_CASE_P(
    Persistent,
    BackgroundFetchDataManagerTest,
    ::testing::Values(BackgroundFetchRegistrationStorage::kPersistent));

INSTANTIATE_TEST_CASE_P(
    NonPersistent,
    BackgroundFetchDataManagerTest,
    ::testing::Values(BackgroundFetchRegistrationStorage::kNonPersistent));

TEST_P(BackgroundFetchDataManagerTest, NoDuplicateRegistrations) {
  // Tests that the BackgroundFetchDataManager correctly rejects creating a
  // registration with a |developer_id| for which there is already an active
  // registration.

  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  BackgroundFetchRegistrationId registration_id1(service_worker_registration_id,
                                                 origin(), kExampleDeveloperId,
                                                 kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests;
  BackgroundFetchOptions options;

  blink::mojom::BackgroundFetchError error;

  // Deactivating the not-yet-created registration should fail.
  MarkRegistrationForDeletion(registration_id1, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  // Creating the initial registration should succeed.
  CreateRegistration(registration_id1, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Different |unique_id|, since this is a new Background Fetch registration,
  // even though it shares the same |developer_id|.
  BackgroundFetchRegistrationId registration_id2(service_worker_registration_id,
                                                 origin(), kExampleDeveloperId,
                                                 kAlternativeUniqueId);

  // Attempting to create a second registration with the same |developer_id| and
  // |service_worker_registration_id| should yield an error.
  CreateRegistration(registration_id2, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID);

  // Deactivating the second registration that failed to be created should fail.
  MarkRegistrationForDeletion(registration_id2, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  // Deactivating the initial registration should succeed.
  MarkRegistrationForDeletion(registration_id1, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // And now registering the second registration should work fine, since there
  // is no longer an *active* registration with the same |developer_id|, even
  // though the initial registration has not yet been deleted.
  CreateRegistration(registration_id2, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
}

TEST_P(BackgroundFetchDataManagerTest, GetDeveloperIds) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  // Verify that no developer IDs can be found.
  auto developer_ids = GetDeveloperIds(sw_id, origin(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, IsEmpty());

  // Create a single registration.
  BackgroundFetchRegistrationId registration_id1(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);
  CreateRegistration(registration_id1, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Verify that the developer ID can be found.
  developer_ids = GetDeveloperIds(sw_id, origin(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, UnorderedElementsAre(kExampleDeveloperId));

  RestartDataManagerFromPersistentStorage();

  // After a restart, GetDeveloperIds should still find the IDs.
  developer_ids = GetDeveloperIds(sw_id, origin(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, UnorderedElementsAre(kExampleDeveloperId));

  // Create another registration.
  BackgroundFetchRegistrationId registration_id2(
      sw_id, origin(), kAlternativeDeveloperId, kAlternativeUniqueId);
  CreateRegistration(registration_id2, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Verify that both developer IDs can be found.
  developer_ids = GetDeveloperIds(sw_id, origin(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, UnorderedElementsAre(kExampleDeveloperId,
                                                  kAlternativeDeveloperId));
  RestartDataManagerFromPersistentStorage();

  // After a restart, GetDeveloperIds should still find the IDs.
  developer_ids = GetDeveloperIds(sw_id, origin(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_THAT(developer_ids, UnorderedElementsAre(kExampleDeveloperId,
                                                  kAlternativeDeveloperId));
}

TEST_P(BackgroundFetchDataManagerTest, GetRegistration) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  // Create a single registration.
  CreateRegistration(registration_id, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Verify that the registration can be retrieved.
  auto registration =
      GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(registration);
  EXPECT_EQ(kExampleUniqueId, registration->unique_id);
  EXPECT_EQ(kExampleDeveloperId, registration->developer_id);

  // Verify that retrieving using the wrong developer id doesn't work.
  registration =
      GetRegistration(sw_id, origin(), kAlternativeDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);
  ASSERT_FALSE(registration);

  RestartDataManagerFromPersistentStorage();

  // After a restart, GetRegistration should still find the registration.
  registration = GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(registration);
  EXPECT_EQ(kExampleUniqueId, registration->unique_id);
  EXPECT_EQ(kExampleDeveloperId, registration->developer_id);
}

TEST_P(BackgroundFetchDataManagerTest, CreateAndDeleteRegistration) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id1(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  CreateRegistration(registration_id1, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  RestartDataManagerFromPersistentStorage();

  // Different |unique_id|, since this is a new Background Fetch registration,
  // even though it shares the same |developer_id|.
  BackgroundFetchRegistrationId registration_id2(
      sw_id, origin(), kExampleDeveloperId, kAlternativeUniqueId);

  // Attempting to create a second registration with the same |developer_id| and
  // |service_worker_registration_id| should yield an error, even after
  // restarting.
  CreateRegistration(registration_id2, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID);

  // Verify that the registration can be retrieved before deletion.
  auto registration =
      GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(registration);
  EXPECT_EQ(kExampleUniqueId, registration->unique_id);
  EXPECT_EQ(kExampleDeveloperId, registration->developer_id);

  // Deactivating the registration should succeed.
  MarkRegistrationForDeletion(registration_id1, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Verify that the registration cannot be retrieved after deletion
  registration = GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);
  ASSERT_FALSE(registration);

  RestartDataManagerFromPersistentStorage();

  // Verify again that the registration cannot be retrieved after deletion and
  // a restart.
  registration = GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);
  ASSERT_FALSE(registration);

  // And now registering the second registration should work fine, even after
  // restarting, since there is no longer an *active* registration with the same
  // |developer_id|, even though the initial registration has not yet been
  // deleted.
  CreateRegistration(registration_id2, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  RestartDataManagerFromPersistentStorage();

  // Deleting the inactive first registration should succeed.
  DeleteRegistration(registration_id1, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
}

TEST_P(BackgroundFetchDataManagerTest, Cleanup) {
  // Tests that the BackgroundFetchDataManager cleans up registrations
  // marked for deletion.

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableBackgroundFetchPersistence);

  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  size_t expected_inactive_data_count =
      kUserDataKeysPerInactiveRegistration +
      requests.size() * kUserDataKeysPerInactiveRequest;

  if (registration_storage_ ==
      BackgroundFetchRegistrationStorage::kPersistent) {
    EXPECT_EQ(
        0u, GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());
  }

  // Create a registration.
  CreateRegistration(registration_id, requests, options, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // And deactivate it.
  MarkRegistrationForDeletion(registration_id, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  RestartDataManagerFromPersistentStorage();

  if (registration_storage_ ==
      BackgroundFetchRegistrationStorage::kPersistent) {
    EXPECT_EQ(
        expected_inactive_data_count,
        GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());
  }

  // Cleanup should delete the registration.
  background_fetch_data_manager_->Cleanup();
  base::RunLoop().RunUntilIdle();
  if (registration_storage_ ==
      BackgroundFetchRegistrationStorage::kPersistent) {
    EXPECT_EQ(
        0u, GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());
  }

  RestartDataManagerFromPersistentStorage();

  // The deletion should have been persisted.
  if (registration_storage_ ==
      BackgroundFetchRegistrationStorage::kPersistent) {
    EXPECT_EQ(
        0u, GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());
  }
}

TEST_P(BackgroundFetchDataManagerTest, CreateInParallel) {
  // Tests that multiple parallel calls to the BackgroundFetchDataManager are
  // linearized and handled one at a time, rather than producing inconsistent
  // results due to interleaving.

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableBackgroundFetchPersistence);

  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  std::vector<ServiceWorkerFetchRequest> requests;
  BackgroundFetchOptions options;

  std::vector<blink::mojom::BackgroundFetchError> errors(5);

  const int num_parallel_creates = 5;

  base::RunLoop run_loop;
  base::RepeatingClosure quit_once_all_finished_closure =
      base::BarrierClosure(num_parallel_creates, run_loop.QuitClosure());
  for (int i = 0; i < num_parallel_creates; i++) {
    // New |unique_id| per iteration, since each is a distinct registration.
    BackgroundFetchRegistrationId registration_id(
        service_worker_registration_id, origin(), kExampleDeveloperId,
        base::GenerateGUID());

    background_fetch_data_manager_->CreateRegistration(
        registration_id, requests, options,
        base::BindOnce(&DidCreateRegistration, quit_once_all_finished_closure,
                       &errors[i]));
  }
  run_loop.Run();

  int success_count = 0;
  int duplicated_developer_id_count = 0;
  for (auto error : errors) {
    switch (error) {
      case blink::mojom::BackgroundFetchError::NONE:
        success_count++;
        break;
      case blink::mojom::BackgroundFetchError::DUPLICATED_DEVELOPER_ID:
        duplicated_developer_id_count++;
        break;
      default:
        break;
    }
  }
  // Exactly one of the calls should have succeeded in creating a registration,
  // and all the others should have failed with DUPLICATED_DEVELOPER_ID.
  EXPECT_EQ(1, success_count);
  EXPECT_EQ(num_parallel_creates - 1, duplicated_developer_id_count);
}

}  // namespace content
