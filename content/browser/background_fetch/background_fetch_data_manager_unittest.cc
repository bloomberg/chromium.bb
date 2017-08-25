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
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"

namespace content {
namespace {

const char kExampleId[] = "my-example-id";

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

  std::unique_ptr<BackgroundFetchDataManager> background_fetch_data_manager_;
};

TEST_F(BackgroundFetchDataManagerTest, NoDuplicateRegistrations) {
  // Tests that the BackgroundFetchDataManager correctly rejects creating a
  // registration that's already known to the system.

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleId, &registration_id));

  std::vector<ServiceWorkerFetchRequest> requests;
  BackgroundFetchOptions options;

  blink::mojom::BackgroundFetchError error;

  // Deleting the not-yet-created registration should fail.
  DeleteRegistration(registration_id, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  // Creating the initial registration should succeed.
  CreateRegistration(registration_id, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Attempting to create it again should yield an error.
  CreateRegistration(registration_id, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::DUPLICATED_ID);

  // Deleting the registration should succeed.
  DeleteRegistration(registration_id, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // And then recreating the registration again should work fine.
  CreateRegistration(registration_id, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
}

TEST_F(BackgroundFetchDataManagerTest, CreateAndDeleteRegistrationPersisted) {
  // Tests that the BackgroundFetchDataManager persists created registrations to
  // the Service Worker DB.

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableBackgroundFetchPersistence);

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleId, &registration_id));

  std::vector<ServiceWorkerFetchRequest> requests;
  BackgroundFetchOptions options;

  blink::mojom::BackgroundFetchError error;

  // Creating the initial registration should succeed.
  CreateRegistration(registration_id, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  RestartDataManagerFromPersistentStorage();

  // Attempting to create it again should yield an error, even after restarting.
  CreateRegistration(registration_id, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::DUPLICATED_ID);

  // Deleting the registration should succeed.
  DeleteRegistration(registration_id, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  RestartDataManagerFromPersistentStorage();

  // And then recreating the registration again should work fine, even after
  // restarting.
  CreateRegistration(registration_id, requests, options, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
}

TEST_F(BackgroundFetchDataManagerTest, CreateInParallel) {
  // Tests that multiple parallel calls to the BackgroundFetchDataManager are
  // linearized and handled one at a time, rather than producing inconsistent
  // results due to interleaving.

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableBackgroundFetchPersistence);

  BackgroundFetchRegistrationId registration_id;
  ASSERT_TRUE(CreateRegistrationId(kExampleId, &registration_id));

  std::vector<ServiceWorkerFetchRequest> requests;
  BackgroundFetchOptions options;

  std::vector<blink::mojom::BackgroundFetchError> errors(5);

  const int num_parallel_creates = 5;

  base::RunLoop run_loop;
  base::RepeatingClosure quit_once_all_finished_closure =
      base::BarrierClosure(num_parallel_creates, run_loop.QuitClosure());
  for (int i = 0; i < num_parallel_creates; i++) {
    background_fetch_data_manager_->CreateRegistration(
        registration_id, requests, options,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidCreateRegistration,
                       base::Unretained(this), quit_once_all_finished_closure,
                       &errors[i]));
  }
  run_loop.Run();

  int success_count = 0;
  int duplicated_id_count = 0;
  for (auto error : errors) {
    switch (error) {
      case blink::mojom::BackgroundFetchError::NONE:
        success_count++;
        break;
      case blink::mojom::BackgroundFetchError::DUPLICATED_ID:
        duplicated_id_count++;
        break;
      default:
        break;
    }
  }
  // Exactly one of the calls should have succeeded in creating a registration,
  // and all the others should have failed with DUPLICATED_ID.
  EXPECT_EQ(1, success_count);
  EXPECT_EQ(num_parallel_creates - 1, duplicated_id_count);
}

}  // namespace content
