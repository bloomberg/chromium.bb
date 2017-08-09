// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/background_fetch_data_manager.h"

#include <memory>
#include <string>

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/public/browser/browser_thread.h"

namespace content {
namespace {

const char kExampleId[] = "my-example-id";

}  // namespace

class BackgroundFetchDataManagerTest : public BackgroundFetchTestBase {
 public:
  BackgroundFetchDataManagerTest()
      : background_fetch_data_manager_(
            base::MakeUnique<BackgroundFetchDataManager>(browser_context())) {}
  ~BackgroundFetchDataManagerTest() override = default;

 protected:
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

 private:
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

  std::string job_guid_;
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
  ASSERT_NO_FATAL_FAILURE(DeleteRegistration(registration_id, &error));
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);

  // Creating the initial registration should succeed.
  ASSERT_NO_FATAL_FAILURE(
      CreateRegistration(registration_id, requests, options, &error));
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Attempting to create it again should yield an error.
  ASSERT_NO_FATAL_FAILURE(
      CreateRegistration(registration_id, requests, options, &error));
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::DUPLICATED_ID);

  // Deleting the registration should succeed.
  ASSERT_NO_FATAL_FAILURE(DeleteRegistration(registration_id, &error));
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // And then recreating the registration again should work fine.
  ASSERT_NO_FATAL_FAILURE(
      CreateRegistration(registration_id, requests, options, &error));
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
}

}  // namespace content
