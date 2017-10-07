// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <memory>
#include <string>

#include "base/barrier_closure.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"

namespace content {
namespace {

const char kExampleDeveloperId[] = "my-example-id";
const char kExampleUniqueId[] = "7e57ab1e-c0de-a150-ca75-1e75f005ba11";
const char kAlternativeUniqueId[] = "bb48a9fb-c21f-4c2d-a9ae-58bd48a9fb53";

// A "bgfetch_registration_developer_id_to_unique_id_" and
// "bgfetch_registration_" per registration (not including keys for requests).
const size_t kUserDataKeysPerRegistration = 2u;
// A "bgfetch_request_" and "bgfetch_pending_request_" per request. See schema
// documentation in background_fetch_data_manager.cc.
const size_t kUserDataKeysPerRequest = 2u;

}  // namespace

class BackgroundFetchDataManagerTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchDataManagerTest() {
    RestartDataManagerFromPersistentStorage();
  }
  ~BackgroundFetchDataManagerTest() override = default;

  // Re-creates the data manager. Useful for testing that data was persisted.
  void RestartDataManagerFromPersistentStorage() {
    background_fetch_data_manager_ =
        base::MakeUnique<BackgroundFetchDataManager>(
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
        base::BindOnce(&BackgroundFetchDataManagerTest::DidCreateRegistration,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error));
    run_loop.Run();
  }

  // Synchronous version of BackgroundFetchDataManager::DeleteRegistration().
  void DeleteRegistration(const BackgroundFetchRegistrationId& registration_id,
                          blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    background_fetch_data_manager_->DeleteRegistration(
        registration_id,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidDeleteRegistration,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error));
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
            base::Bind(&BackgroundFetchDataManagerTest::
                           DidGetRegistrationUserDataByKeyPrefix,
                       base::Unretained(this), run_loop.QuitClosure(), &data));
    run_loop.Run();

    return data;
  }

  void DidCreateRegistration(base::Closure quit_closure,
                             blink::mojom::BackgroundFetchError* out_error,
                             blink::mojom::BackgroundFetchError error) {
    *out_error = error;

    quit_closure.Run();
  }

  void DidDeleteRegistration(base::Closure quit_closure,
                             blink::mojom::BackgroundFetchError* out_error,
                             blink::mojom::BackgroundFetchError error) {
    *out_error = error;

    quit_closure.Run();
  }

 protected:
  std::unique_ptr<BackgroundFetchDataManager> background_fetch_data_manager_;

 private:
  void DidGetRegistrationUserDataByKeyPrefix(
      base::Closure quit_closure,
      std::vector<std::string>* out_data,
      const std::vector<std::string>& data,
      ServiceWorkerStatusCode status) {
    DCHECK(out_data);
    DCHECK_EQ(SERVICE_WORKER_OK, status);
    *out_data = data;
    quit_closure.Run();
  }
};

TEST_F(BackgroundFetchDataManagerTest, NoDuplicateRegistrations) {
  // Tests that the BackgroundFetchDataManager correctly rejects creating a
  // registration that's already known to the system.

  int64_t service_worker_registration_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId,
            service_worker_registration_id);

  BackgroundFetchRegistrationId registration_id1(service_worker_registration_id,
                                                 origin(), kExampleDeveloperId,
                                                 kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests;
  BackgroundFetchOptions options;

  blink::mojom::BackgroundFetchError error;

  // Deleting the not-yet-created registration should fail.
  DeleteRegistration(registration_id1, &error);
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

  // Deleting the second registration that failed to be created should fail.
  DeleteRegistration(registration_id2, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  // Deleting the initial registration should succeed.
  DeleteRegistration(registration_id1, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // And now registering the second registration should work fine.
  CreateRegistration(registration_id2, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
}

TEST_F(BackgroundFetchDataManagerTest, CreateAndDeleteRegistrationPersisted) {
  // Tests that the BackgroundFetchDataManager persists created registrations to
  // the Service Worker DB.

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableBackgroundFetchPersistence);

  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id1(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  size_t expected_data_count =
      kUserDataKeysPerRegistration + requests.size() * kUserDataKeysPerRequest;

  EXPECT_EQ(0u, GetRegistrationUserDataByKeyPrefix(sw_id, "bgfetch_").size());

  // Creating the initial registration should succeed.
  CreateRegistration(registration_id1, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(expected_data_count,
            GetRegistrationUserDataByKeyPrefix(sw_id, "bgfetch_").size());

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

  // Deleting the registration should succeed.
  DeleteRegistration(registration_id1, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(0u, GetRegistrationUserDataByKeyPrefix(sw_id, "bgfetch_").size());

  RestartDataManagerFromPersistentStorage();

  // And now registering the second registration should work fine, even after
  // restarting.
  CreateRegistration(registration_id2, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(expected_data_count,
            GetRegistrationUserDataByKeyPrefix(sw_id, "bgfetch_").size());
}

TEST_F(BackgroundFetchDataManagerTest, CreateInParallel) {
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
        base::BindOnce(&BackgroundFetchDataManagerTest::DidCreateRegistration,
                       base::Unretained(this), quit_once_all_finished_closure,
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
