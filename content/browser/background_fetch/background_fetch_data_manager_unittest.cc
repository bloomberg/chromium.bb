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
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/background_fetch/background_fetch_data_manager_observer.h"
#include "content/browser/background_fetch/background_fetch_request_info.h"
#include "content/browser/background_fetch/background_fetch_test_base.h"
#include "content/browser/background_fetch/background_fetch_test_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {
namespace {

using background_fetch::BackgroundFetchInitializationData;
using ::testing::UnorderedElementsAre;
using ::testing::IsEmpty;

const char kUserDataPrefix[] = "bgfetch_";

const char kExampleDeveloperId[] = "my-example-id";
const char kAlternativeDeveloperId[] = "my-other-id";
const char kExampleUniqueId[] = "7e57ab1e-c0de-a150-ca75-1e75f005ba11";
const char kAlternativeUniqueId[] = "bb48a9fb-c21f-4c2d-a9ae-58bd48a9fb53";

const char kInitialTitle[] = "Initial Title";
const char kUpdatedTitle[] = "Updated Title";

constexpr size_t kResponseFileSize = 42u;

void DidGetInitializationData(
    base::Closure quit_closure,
    std::vector<BackgroundFetchInitializationData>* out_result,
    blink::mojom::BackgroundFetchError error,
    std::vector<BackgroundFetchInitializationData> result) {
  DCHECK_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  *out_result = std::move(result);
  std::move(quit_closure).Run();
}

void DidCreateRegistration(
    base::Closure quit_closure,
    blink::mojom::BackgroundFetchError* out_error,
    blink::mojom::BackgroundFetchError error,
    std::unique_ptr<BackgroundFetchRegistration> registration) {
  *out_error = error;
  std::move(quit_closure).Run();
}

void DidGetError(base::Closure quit_closure,
                 blink::mojom::BackgroundFetchError* out_error,
                 blink::mojom::BackgroundFetchError error) {
  *out_error = error;
  std::move(quit_closure).Run();
}

void DidGetRegistrationUserDataByKeyPrefix(
    base::OnceClosure quit_closure,
    std::vector<std::string>* out_data,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  DCHECK(out_data);
  DCHECK_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  *out_data = data;
  std::move(quit_closure).Run();
}

void AnnotateRequestInfoWithFakeDownloadManagerData(
    BackgroundFetchRequestInfo* request_info,
    bool success = false) {
  DCHECK(request_info);

  std::string headers =
      success ? "HTTP/1.1 200 OK\n" : "HTTP/1.1 404 Not found\n";
  request_info->PopulateWithResponse(std::make_unique<BackgroundFetchResponse>(
      std::vector<GURL>(1u, request_info->fetch_request().url),
      base::MakeRefCounted<net::HttpResponseHeaders>(headers)));

  if (!success) {
    // Fill |request_info| with a failed result.
    request_info->SetResult(std::make_unique<BackgroundFetchResult>(
        base::Time::Now(), BackgroundFetchResult::FailureReason::UNKNOWN));
    return;
  }

  // This is treated as an empty response, but the size is set to
  // |kResponseFileSize| for tests that use filesize.
  request_info->SetResult(std::make_unique<BackgroundFetchResult>(
      base::Time::Now(), base::FilePath(), base::nullopt /* blob_handle */,
      kResponseFileSize));
}

void GetNumUserData(base::Closure quit_closure,
                    int* out_size,
                    const std::vector<std::string>& data,
                    blink::ServiceWorkerStatusCode status) {
  DCHECK(out_size);
  DCHECK_EQ(blink::ServiceWorkerStatusCode::kOk, status);
  *out_size = data.size();
  std::move(quit_closure).Run();
}

struct ResponseStateStats {
  int pending_requests = 0;
  int active_requests = 0;
  int completed_requests = 0;
};

bool operator==(const ResponseStateStats& s1, const ResponseStateStats& s2) {
  return s1.pending_requests == s2.pending_requests &&
         s1.active_requests == s2.active_requests &&
         s1.completed_requests == s2.completed_requests;
}

std::vector<ServiceWorkerFetchRequest> CreateValidRequests(
    const url::Origin& origin,
    size_t num_requests = 1u) {
  std::vector<ServiceWorkerFetchRequest> requests(num_requests);
  for (size_t i = 0; i < requests.size(); i++) {
    // Creates a URL of the form: `http://example.com/x`
    requests[i].url = GURL(origin.GetURL().spec() + base::NumberToString(i));
  }
  return requests;
}

}  // namespace

class BackgroundFetchDataManagerTest
    : public BackgroundFetchTestBase,
      public BackgroundFetchDataManagerObserver {
 public:
  BackgroundFetchDataManagerTest() {
    RestartDataManagerFromPersistentStorage();
  }

  ~BackgroundFetchDataManagerTest() override {
    background_fetch_data_manager_->RemoveObserver(this);
  }

  // Re-creates the data manager. Useful for testing that data was persisted.
  void RestartDataManagerFromPersistentStorage() {
    background_fetch_data_manager_ =
        std::make_unique<BackgroundFetchTestDataManager>(
            browser_context(), storage_partition(),
            embedded_worker_test_helper()->context_wrapper(),
            true /* mock_fill_response */);

    background_fetch_data_manager_->AddObserver(this);
    background_fetch_data_manager_->InitializeOnIOThread();
  }

  // Synchronous version of BackgroundFetchDataManager::GetInitializationData().
  std::vector<BackgroundFetchInitializationData> GetInitializationData() {
    // Simulate browser restart. This re-initializes |data_manager_|, since
    // this DatabaseTask should only be called on browser startup.
    RestartDataManagerFromPersistentStorage();

    std::vector<BackgroundFetchInitializationData> result;

    base::RunLoop run_loop;
    background_fetch_data_manager_->GetInitializationData(base::BindOnce(
        &DidGetInitializationData, run_loop.QuitClosure(), &result));
    run_loop.Run();

    return result;
  }

  // Synchronous version of BackgroundFetchDataManager::CreateRegistration().
  void CreateRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      const std::vector<ServiceWorkerFetchRequest>& requests,
      const BackgroundFetchOptions& options,
      const SkBitmap& icon,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    background_fetch_data_manager_->CreateRegistration(
        registration_id, requests, options, icon,
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

  std::unique_ptr<proto::BackgroundFetchMetadata> GetMetadata(
      int64_t service_worker_registration_id,
      const url::Origin& origin,
      const std::string developer_id,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    std::unique_ptr<proto::BackgroundFetchMetadata> metadata;
    base::RunLoop run_loop;
    background_fetch_data_manager_->GetMetadata(
        service_worker_registration_id, origin, developer_id,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidGetMetadata,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, &metadata));
    run_loop.Run();

    return metadata;
  }

  void UpdateRegistrationUI(
      const BackgroundFetchRegistrationId& registration_id,
      const std::string& updated_title,
      blink::mojom::BackgroundFetchError* out_error) {
    DCHECK(out_error);

    base::RunLoop run_loop;
    background_fetch_data_manager_->UpdateRegistrationUI(
        registration_id, updated_title,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidUpdateRegistrationUI,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error));
    run_loop.Run();
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
  // BackgroundFetchDataManager::PopNextRequest().
  void PopNextRequest(
      const BackgroundFetchRegistrationId& registration_id,
      scoped_refptr<BackgroundFetchRequestInfo>* out_request_info) {
    DCHECK(out_request_info);

    base::RunLoop run_loop;
    background_fetch_data_manager_->PopNextRequest(
        registration_id,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidPopNextRequest,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_request_info));
    run_loop.Run();
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

  // Synchronous version of BackgroundFetchDataManager::MarkRequestAsComplete().
  void MarkRequestAsComplete(
      const BackgroundFetchRegistrationId& registration_id,
      BackgroundFetchRequestInfo* request_info) {
    base::RunLoop run_loop;
    background_fetch_data_manager_->MarkRequestAsComplete(
        registration_id, request_info,
        base::BindOnce(
            &BackgroundFetchDataManagerTest::DidMarkRequestAsComplete,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Synchronous version of
  // BackgroundFetchDataManager::GetSettledFetchesForRegistration().
  void GetSettledFetchesForRegistration(
      const BackgroundFetchRegistrationId& registration_id,
      blink::mojom::BackgroundFetchError* out_error,
      bool* out_succeeded,
      std::vector<BackgroundFetchSettledFetch>* out_settled_fetches) {
    DCHECK(out_error);
    DCHECK(out_succeeded);
    DCHECK(out_settled_fetches);

    base::RunLoop run_loop;
    background_fetch_data_manager_->GetSettledFetchesForRegistration(
        registration_id,
        base::BindOnce(&BackgroundFetchDataManagerTest::
                           DidGetSettledFetchesForRegistration,
                       base::Unretained(this), run_loop.QuitClosure(),
                       out_error, out_succeeded, out_settled_fetches));
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
            base::BindOnce(&DidGetRegistrationUserDataByKeyPrefix,
                           run_loop.QuitClosure(), &data));
    run_loop.Run();

    return data;
  }

  // Synchronous version of CacheStorageManager::HasCache().
  bool HasCache(const std::string& cache_name) {
    bool result = false;

    base::RunLoop run_loop;
    background_fetch_data_manager_->cache_manager()->HasCache(
        origin(), CacheStorageOwner::kBackgroundFetch, cache_name,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidFindCache,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();

    return result;
  }

  // Synchronous version of CacheStorageManager::MatchCache().
  bool MatchCache(const ServiceWorkerFetchRequest& request) {
    bool result = false;
    auto request_ptr = std::make_unique<ServiceWorkerFetchRequest>(request);

    base::RunLoop run_loop;
    background_fetch_data_manager_->cache_manager()->MatchCache(
        origin(), CacheStorageOwner::kBackgroundFetch, kExampleUniqueId,
        std::move(request_ptr), nullptr /* match_params */,
        base::BindOnce(&BackgroundFetchDataManagerTest::DidMatchCache,
                       base::Unretained(this), run_loop.QuitClosure(),
                       &result));
    run_loop.Run();

    return result;
  }

  // Gets information about the number of background fetch requests by state.
  ResponseStateStats GetRequestStats(int64_t service_worker_registration_id) {
    ResponseStateStats stats;
    {
      base::RunLoop run_loop;
      embedded_worker_test_helper()
          ->context_wrapper()
          ->GetRegistrationUserDataByKeyPrefix(
              service_worker_registration_id,
              background_fetch::kPendingRequestKeyPrefix,
              base::BindOnce(&GetNumUserData, run_loop.QuitClosure(),
                             &stats.pending_requests));
      run_loop.Run();
    }
    {
      base::RunLoop run_loop;
      embedded_worker_test_helper()
          ->context_wrapper()
          ->GetRegistrationUserDataByKeyPrefix(
              service_worker_registration_id,
              background_fetch::kActiveRequestKeyPrefix,
              base::BindOnce(&GetNumUserData, run_loop.QuitClosure(),
                             &stats.active_requests));
      run_loop.Run();
    }
    {
      base::RunLoop run_loop;
      embedded_worker_test_helper()
          ->context_wrapper()
          ->GetRegistrationUserDataByKeyPrefix(
              service_worker_registration_id,
              background_fetch::kCompletedRequestKeyPrefix,
              base::BindOnce(&GetNumUserData, run_loop.QuitClosure(),
                             &stats.completed_requests));
      run_loop.Run();
    }
    return stats;
  }

  // BackgroundFetchDataManagerObserver mocks:
  MOCK_METHOD2(OnUpdatedUI,
               void(const BackgroundFetchRegistrationId& registration,
                    const std::string& title));
  MOCK_METHOD1(OnServiceWorkerDatabaseCorrupted,
               void(int64_t service_worker_registration_id));

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

    std::move(quit_closure).Run();
  }

  void DidGetMetadata(
      base::OnceClosure quit_closure,
      blink::mojom::BackgroundFetchError* out_error,
      std::unique_ptr<proto::BackgroundFetchMetadata>* out_metadata,
      blink::mojom::BackgroundFetchError error,
      std::unique_ptr<proto::BackgroundFetchMetadata> metadata) {
    if (error == blink::mojom::BackgroundFetchError::NONE) {
      DCHECK(metadata);
    }
    *out_error = error;
    *out_metadata = std::move(metadata);

    std::move(quit_closure).Run();
  }

  void DidUpdateRegistrationUI(base::OnceClosure quit_closure,
                               blink::mojom::BackgroundFetchError* out_error,
                               blink::mojom::BackgroundFetchError error) {
    *out_error = error;
    std::move(quit_closure).Run();
  }

  void DidGetDeveloperIds(base::Closure quit_closure,
                          blink::mojom::BackgroundFetchError* out_error,
                          std::vector<std::string>* out_ids,
                          blink::mojom::BackgroundFetchError error,
                          const std::vector<std::string>& ids) {
    *out_error = error;
    *out_ids = ids;

    std::move(quit_closure).Run();
  }

  void DidPopNextRequest(
      base::OnceClosure quit_closure,
      scoped_refptr<BackgroundFetchRequestInfo>* out_request_info,
      scoped_refptr<BackgroundFetchRequestInfo> request_info) {
    *out_request_info = request_info;
    std::move(quit_closure).Run();
  }

  void DidMarkRequestAsComplete(base::OnceClosure quit_closure) {
    std::move(quit_closure).Run();
  }

  void DidGetSettledFetchesForRegistration(
      base::OnceClosure quit_closure,
      blink::mojom::BackgroundFetchError* out_error,
      bool* out_succeeded,
      std::vector<BackgroundFetchSettledFetch>* out_settled_fetches,
      blink::mojom::BackgroundFetchError error,
      bool succeeded,
      std::vector<BackgroundFetchSettledFetch> settled_fetches,
      std::vector<std::unique_ptr<storage::BlobDataHandle>>) {
    *out_error = error;
    *out_succeeded = succeeded;
    *out_settled_fetches = std::move(settled_fetches);

    std::move(quit_closure).Run();
  }

  void DidFindCache(base::OnceClosure quit_closure,
                    bool* out_result,
                    bool has_cache,
                    blink::mojom::CacheStorageError error) {
    DCHECK_EQ(error, blink::mojom::CacheStorageError::kSuccess);
    *out_result = has_cache;
    std::move(quit_closure).Run();
  }

  void DidMatchCache(base::OnceClosure quit_closure,
                     bool* out_result,
                     blink::mojom::CacheStorageError error,
                     std::unique_ptr<ServiceWorkerResponse> response) {
    if (error == blink::mojom::CacheStorageError::kSuccess) {
      DCHECK(response);
      *out_result = true;
    } else {
      *out_result = false;
    }
    std::move(quit_closure).Run();
  }

  std::unique_ptr<BackgroundFetchTestDataManager>
      background_fetch_data_manager_;
};

TEST_F(BackgroundFetchDataManagerTest, NoDuplicateRegistrations) {
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
  CreateRegistration(registration_id1, requests, options, SkBitmap(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Different |unique_id|, since this is a new Background Fetch registration,
  // even though it shares the same |developer_id|.
  BackgroundFetchRegistrationId registration_id2(service_worker_registration_id,
                                                 origin(), kExampleDeveloperId,
                                                 kAlternativeUniqueId);

  // Attempting to create a second registration with the same |developer_id| and
  // |service_worker_registration_id| should yield an error.
  CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
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
  CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
}

TEST_F(BackgroundFetchDataManagerTest, GetDeveloperIds) {
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
  CreateRegistration(registration_id1, requests, options, SkBitmap(), &error);
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
  CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
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

TEST_F(BackgroundFetchDataManagerTest, GetRegistration) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  // Create a single registration.
  CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
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

TEST_F(BackgroundFetchDataManagerTest, GetMetadata) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  // Create a single registration.
  CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Verify that the metadata can be retrieved.
  auto metadata = GetMetadata(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->origin(), origin().Serialize());
  EXPECT_NE(metadata->creation_microseconds_since_unix_epoch(), 0);
  EXPECT_EQ(metadata->num_fetches(), static_cast<int>(requests.size()));

  // Verify that retrieving using the wrong developer id doesn't work.
  metadata = GetMetadata(sw_id, origin(), kAlternativeDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::INVALID_ID);
  ASSERT_FALSE(metadata);

  RestartDataManagerFromPersistentStorage();

  // After a restart, GetMetadata should still find the registration.
  metadata = GetMetadata(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->origin(), origin().Serialize());
  EXPECT_NE(metadata->creation_microseconds_since_unix_epoch(), 0);
  EXPECT_EQ(metadata->num_fetches(), static_cast<int>(requests.size()));
}

TEST_F(BackgroundFetchDataManagerTest, LargeIconNotPersisted) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  SkBitmap icon;
  icon.allocN32Pixels(512, 512);
  icon.eraseColor(SK_ColorGREEN);

  // Create a single registration.
  CreateRegistration(registration_id, requests, options, icon, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Verify that the metadata can be retrieved.
  auto metadata = GetMetadata(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->origin(), origin().Serialize());
  EXPECT_NE(metadata->creation_microseconds_since_unix_epoch(), 0);
  EXPECT_EQ(metadata->num_fetches(), static_cast<int>(requests.size()));
  EXPECT_TRUE(metadata->icon().empty());
}

TEST_F(BackgroundFetchDataManagerTest, UpdateRegistrationUI) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  options.title = kInitialTitle;
  blink::mojom::BackgroundFetchError error;

  // There should be no title before the registration.
  std::vector<std::string> title = GetRegistrationUserDataByKeyPrefix(
      sw_id, background_fetch::kTitleKeyPrefix);
  EXPECT_TRUE(title.empty());

  // Create a single registration.
  CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // Verify that the title can be retrieved.
  title = GetRegistrationUserDataByKeyPrefix(sw_id,
                                             background_fetch::kTitleKeyPrefix);
  EXPECT_EQ(title.size(), 1u);
  ASSERT_EQ(title.front(), kInitialTitle);

  // Update the title.
  {
    EXPECT_CALL(*this, OnUpdatedUI(registration_id, kUpdatedTitle));

    UpdateRegistrationUI(registration_id, kUpdatedTitle, &error);
    ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  }

  RestartDataManagerFromPersistentStorage();

  // After a restart, GetMetadata should find the new title.
  title = GetRegistrationUserDataByKeyPrefix(sw_id,
                                             background_fetch::kTitleKeyPrefix);
  EXPECT_EQ(title.size(), 1u);
  ASSERT_EQ(title.front(), kUpdatedTitle);
}

TEST_F(BackgroundFetchDataManagerTest, CreateAndDeleteRegistration) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id1(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  CreateRegistration(registration_id1, requests, options, SkBitmap(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  RestartDataManagerFromPersistentStorage();

  // Different |unique_id|, since this is a new Background Fetch registration,
  // even though it shares the same |developer_id|.
  BackgroundFetchRegistrationId registration_id2(
      sw_id, origin(), kExampleDeveloperId, kAlternativeUniqueId);

  // Attempting to create a second registration with the same |developer_id| and
  // |service_worker_registration_id| should yield an error, even after
  // restarting.
  CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
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
  CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  RestartDataManagerFromPersistentStorage();

  // Deleting the inactive first registration should succeed.
  DeleteRegistration(registration_id1, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
}

TEST_F(BackgroundFetchDataManagerTest, PopNextRequestAndMarkAsComplete) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  scoped_refptr<BackgroundFetchRequestInfo> request_info;

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  // There registration hasn't been created yet, so there are no pending
  // requests.
  PopNextRequest(registration_id, &request_info);
  EXPECT_FALSE(request_info);
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{0 /* pending_requests */, 0 /* active_requests */,
                          0 /* completed_requests */}));

  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{2 /* pending_requests */, 0 /* active_requests */,
                          0 /* completed_requests */}));

  // Popping should work now.
  PopNextRequest(registration_id, &request_info);
  EXPECT_TRUE(request_info);
  EXPECT_EQ(request_info->request_index(), 0);
  EXPECT_FALSE(request_info->download_guid().empty());
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{1 /* pending_requests */, 1 /* active_requests */,
                          0 /* completed_requests */}));

  // Mark as complete.
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
  MarkRequestAsComplete(registration_id, request_info.get());
  ASSERT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{1 /* pending_requests */, 0 /* active_requests */,
                          1 /* completed_requests */}));

  RestartDataManagerFromPersistentStorage();

  PopNextRequest(registration_id, &request_info);
  EXPECT_TRUE(request_info);
  EXPECT_EQ(request_info->request_index(), 1);
  EXPECT_FALSE(request_info->download_guid().empty());
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{0 /* pending_requests */, 1 /* active_requests */,
                          1 /* completed_requests */}));

  // Mark as complete.
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
  MarkRequestAsComplete(registration_id, request_info.get());
  ASSERT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{0 /* pending_requests */, 0 /* active_requests */,
                          2 /* completed_requests */}));

  // We are out of pending requests.
  PopNextRequest(registration_id, &request_info);
  EXPECT_FALSE(request_info);
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{0 /* pending_requests */, 0 /* active_requests */,
                          2 /* completed_requests */}));
}

TEST_F(BackgroundFetchDataManagerTest, DownloadTotalUpdated) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);
  auto requests = CreateValidRequests(origin(), 3u /* num_requests */);

  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  auto registration =
      GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(registration->download_total, 0u);

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &request_info);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* succeeded */);
  MarkRequestAsComplete(registration_id, request_info.get());

  registration = GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(registration->download_total, kResponseFileSize);

  PopNextRequest(registration_id, &request_info);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* succeeded */);
  MarkRequestAsComplete(registration_id, request_info.get());

  registration = GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(registration->download_total, 2 * kResponseFileSize);

  PopNextRequest(registration_id, &request_info);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 false /* succeeded */);
  MarkRequestAsComplete(registration_id, request_info.get());

  registration = GetRegistration(sw_id, origin(), kExampleDeveloperId, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  // |download_total| is unchanged.
  EXPECT_EQ(registration->download_total, 2 * kResponseFileSize);
}

TEST_F(BackgroundFetchDataManagerTest, WriteToCache) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);
  auto requests = CreateValidRequests(origin(), 2u /* num_requests */);

  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &request_info);
  ASSERT_TRUE(request_info);

  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* success */);
  MarkRequestAsComplete(registration_id, request_info.get());

  EXPECT_TRUE(HasCache(kExampleUniqueId));
  EXPECT_FALSE(HasCache("foo"));

  EXPECT_TRUE(MatchCache(requests[0]));
  EXPECT_FALSE(MatchCache(requests[1]));

  PopNextRequest(registration_id, &request_info);
  ASSERT_TRUE(request_info);

  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* success */);
  MarkRequestAsComplete(registration_id, request_info.get());
  EXPECT_TRUE(MatchCache(requests[0]));
  EXPECT_TRUE(MatchCache(requests[1]));

  RestartDataManagerFromPersistentStorage();
  EXPECT_TRUE(MatchCache(requests[0]));
  EXPECT_TRUE(MatchCache(requests[1]));
}

TEST_F(BackgroundFetchDataManagerTest, CacheDeleted) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  ServiceWorkerFetchRequest request;
  request.url = GURL(origin().GetURL().spec());

  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  CreateRegistration(registration_id, {request}, options, SkBitmap(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &request_info);
  ASSERT_TRUE(request_info);

  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* success */);
  MarkRequestAsComplete(registration_id, request_info.get());

  EXPECT_TRUE(HasCache(kExampleUniqueId));
  EXPECT_TRUE(MatchCache(request));

  MarkRegistrationForDeletion(registration_id, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  DeleteRegistration(registration_id, &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  EXPECT_FALSE(HasCache(kExampleUniqueId));
}

TEST_F(BackgroundFetchDataManagerTest, GetSettledFetchesForRegistration) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{2 /* pending_requests */, 0 /* active_requests */,
                          0 /* completed_requests */}));

  // Nothing is downloaded yet.
  bool succeeded = false;
  std::vector<BackgroundFetchSettledFetch> settled_fetches;
  GetSettledFetchesForRegistration(registration_id, &error, &succeeded,
                                   &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(settled_fetches.size(), 0u);

  for (size_t i = 0; i < requests.size(); i++) {
    scoped_refptr<BackgroundFetchRequestInfo> request_info;
    PopNextRequest(registration_id, &request_info);
    ASSERT_TRUE(request_info);
    AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
    MarkRequestAsComplete(registration_id, request_info.get());
  }

  RestartDataManagerFromPersistentStorage();

  EXPECT_EQ(
      GetRequestStats(sw_id),
      (ResponseStateStats{0 /* pending_requests */, 0 /* active_requests */,
                          requests.size() /* completed_requests */}));

  GetSettledFetchesForRegistration(registration_id, &error, &succeeded,
                                   &settled_fetches);

  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  // We are marking the responses as failed in Download Manager.
  EXPECT_FALSE(succeeded);
  EXPECT_EQ(settled_fetches.size(), requests.size());
}

TEST_F(BackgroundFetchDataManagerTest, GetSettledFetchesFromCache) {
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);
  auto requests = CreateValidRequests(origin(), 2u /* num_requests */);

  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;
  CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  bool succeeded = false;
  std::vector<BackgroundFetchSettledFetch> settled_fetches;
  // Nothing is downloaded yet.
  GetSettledFetchesForRegistration(registration_id, &error, &succeeded,
                                   &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(settled_fetches.size(), 0u);

  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &request_info);
  ASSERT_TRUE(request_info);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* success */);
  MarkRequestAsComplete(registration_id, request_info.get());

  GetSettledFetchesForRegistration(registration_id, &error, &succeeded,
                                   &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(settled_fetches.size(), 1u);

  PopNextRequest(registration_id, &request_info);
  ASSERT_TRUE(request_info);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get(),
                                                 true /* success */);
  MarkRequestAsComplete(registration_id, request_info.get());

  GetSettledFetchesForRegistration(registration_id, &error, &succeeded,
                                   &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_TRUE(succeeded);
  ASSERT_EQ(settled_fetches.size(), 2u);

  // Sanity check that the responses are written to / read from the cache.
  EXPECT_TRUE(MatchCache(requests[0]));
  EXPECT_TRUE(MatchCache(requests[1]));
  EXPECT_EQ(settled_fetches[0].response.cache_storage_cache_name,
            kExampleUniqueId);
  EXPECT_EQ(settled_fetches[1].response.cache_storage_cache_name,
            kExampleUniqueId);

  RestartDataManagerFromPersistentStorage();

  GetSettledFetchesForRegistration(registration_id, &error, &succeeded,
                                   &settled_fetches);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  EXPECT_TRUE(succeeded);
  EXPECT_EQ(settled_fetches.size(), 2u);
}

TEST_F(BackgroundFetchDataManagerTest, Cleanup) {
  // Tests that the BackgroundFetchDataManager cleans up registrations
  // marked for deletion.
  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);

  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);

  // The requests are default-initialized, but valid.
  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  blink::mojom::BackgroundFetchError error;

  EXPECT_EQ(0u,
            GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());
  // Create a registration.
  CreateRegistration(registration_id, requests, options, SkBitmap(), &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  // We expect as many pending entries as there are requests.
  EXPECT_EQ(requests.size(),
            GetRegistrationUserDataByKeyPrefix(
                sw_id, background_fetch::kPendingRequestKeyPrefix)
                .size());

  // And deactivate it.
  MarkRegistrationForDeletion(registration_id, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);

  RestartDataManagerFromPersistentStorage();

  // Pending Requests should be deleted after marking a registration for
  // deletion.
  EXPECT_EQ(0u, GetRegistrationUserDataByKeyPrefix(
                    sw_id, background_fetch::kPendingRequestKeyPrefix)
                    .size());
  EXPECT_EQ(2u,  // Metadata proto + title.
            GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());

  // Cleanup should delete the registration.
  background_fetch_data_manager_->Cleanup();
  thread_bundle_.RunUntilIdle();
  EXPECT_EQ(0u,
            GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());

  RestartDataManagerFromPersistentStorage();

  // The deletion should have been persisted.
  EXPECT_EQ(0u,
            GetRegistrationUserDataByKeyPrefix(sw_id, kUserDataPrefix).size());
}

TEST_F(BackgroundFetchDataManagerTest, GetInitializationData) {
  {
    // No registered ServiceWorkers.
    std::vector<BackgroundFetchInitializationData> data =
        GetInitializationData();
    EXPECT_TRUE(data.empty());
  }

  int64_t sw_id = RegisterServiceWorker();
  ASSERT_NE(blink::mojom::kInvalidServiceWorkerRegistrationId, sw_id);
  {
    // Register ServiceWorker with no Background Fetch Registrations.
    std::vector<BackgroundFetchInitializationData> data =
        GetInitializationData();
    EXPECT_TRUE(data.empty());
  }

  std::vector<ServiceWorkerFetchRequest> requests(2u);
  BackgroundFetchOptions options;
  options.title = kInitialTitle;
  options.download_total = 42u;
  blink::mojom::BackgroundFetchError error;
  // Register a Background Fetch.
  BackgroundFetchRegistrationId registration_id(
      sw_id, origin(), kExampleDeveloperId, kExampleUniqueId);
  SkBitmap icon;
  icon.allocN32Pixels(42, 42);
  icon.eraseColor(SK_ColorGREEN);
  CreateRegistration(registration_id, requests, options, icon, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  UpdateRegistrationUI(registration_id, kUpdatedTitle, &error);
  ASSERT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  {
    std::vector<BackgroundFetchInitializationData> data =
        GetInitializationData();
    ASSERT_EQ(data.size(), 1u);
    const BackgroundFetchInitializationData& init = data[0];

    EXPECT_EQ(init.registration_id, registration_id);
    EXPECT_EQ(init.registration.unique_id, kExampleUniqueId);
    EXPECT_EQ(init.registration.developer_id, kExampleDeveloperId);
    EXPECT_EQ(init.options.title, kInitialTitle);
    EXPECT_EQ(init.options.download_total, 42u);
    EXPECT_EQ(init.ui_title, kUpdatedTitle);
    EXPECT_EQ(init.num_requests, requests.size());
    EXPECT_EQ(init.num_completed_requests, 0u);
    EXPECT_TRUE(init.active_fetch_guids.empty());

    // Check icon.
    ASSERT_FALSE(init.icon.drawsNothing());
    EXPECT_EQ(icon.width(), init.icon.width());
    EXPECT_EQ(icon.height(), init.icon.height());
    for (int i = 0; i < icon.width(); i++) {
      for (int j = 0; j < icon.height(); j++)
        EXPECT_EQ(init.icon.getColor(i, j), SK_ColorGREEN);
    }
  }

  // Mark one request as complete and start another.
  scoped_refptr<BackgroundFetchRequestInfo> request_info;
  PopNextRequest(registration_id, &request_info);
  ASSERT_TRUE(request_info);
  AnnotateRequestInfoWithFakeDownloadManagerData(request_info.get());
  MarkRequestAsComplete(registration_id, request_info.get());
  PopNextRequest(registration_id, &request_info);
  ASSERT_TRUE(request_info);
  {
    std::vector<BackgroundFetchInitializationData> data =
        GetInitializationData();
    ASSERT_EQ(data.size(), 1u);

    EXPECT_EQ(data[0].num_requests, requests.size());
    EXPECT_EQ(data[0].num_completed_requests, 1u);
    EXPECT_EQ(data[0].active_fetch_guids.size(), 1u);
  }

  // Create another registration.
  BackgroundFetchRegistrationId registration_id2(
      sw_id, origin(), kAlternativeDeveloperId, kAlternativeUniqueId);
  CreateRegistration(registration_id2, requests, options, SkBitmap(), &error);
  EXPECT_EQ(error, blink::mojom::BackgroundFetchError::NONE);
  {
    std::vector<BackgroundFetchInitializationData> data =
        GetInitializationData();
    ASSERT_EQ(data.size(), 2u);
  }
}

TEST_F(BackgroundFetchDataManagerTest, CreateInParallel) {
  // Tests that multiple parallel calls to the BackgroundFetchDataManager are
  // linearized and handled one at a time, rather than producing inconsistent
  // results due to interleaving.
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
        registration_id, requests, options, SkBitmap(),
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
