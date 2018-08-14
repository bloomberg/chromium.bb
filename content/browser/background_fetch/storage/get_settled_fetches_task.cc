// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_settled_fetches_task.h"

#include "base/barrier_closure.h"
#include "content/browser/background_fetch/background_fetch_data_manager.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"

namespace content {

namespace background_fetch {

GetSettledFetchesTask::GetSettledFetchesTask(
    DatabaseTaskHost* host,
    BackgroundFetchRegistrationId registration_id,
    std::unique_ptr<BackgroundFetchRequestMatchParams> match_params,
    SettledFetchesCallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      match_params_(std::move(match_params)),
      settled_fetches_callback_(std::move(callback)),
      weak_factory_(this) {}

GetSettledFetchesTask::~GetSettledFetchesTask() = default;

void GetSettledFetchesTask::Start() {
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      2u, base::BindOnce(&GetSettledFetchesTask::GetResponses,
                         weak_factory_.GetWeakPtr()));

  cache_manager()->OpenCache(
      registration_id_.origin(), CacheStorageOwner::kBackgroundFetch,
      registration_id_.unique_id() /* cache_name */,
      base::BindOnce(&GetSettledFetchesTask::DidOpenCache,
                     weak_factory_.GetWeakPtr(), barrier_closure));

  service_worker_context()->GetRegistrationUserDataByKeyPrefix(
      registration_id_.service_worker_registration_id(),
      {CompletedRequestKeyPrefix(registration_id_.unique_id())},
      base::BindOnce(&GetSettledFetchesTask::DidGetCompletedRequests,
                     weak_factory_.GetWeakPtr(), barrier_closure));
}

void GetSettledFetchesTask::DidOpenCache(
    base::OnceClosure done_closure,
    CacheStorageCacheHandle handle,
    blink::mojom::CacheStorageError error) {
  if (error != blink::mojom::CacheStorageError::kSuccess) {
    SetStorageError(BackgroundFetchStorageError::kCacheStorageError);
  } else {
    DCHECK(handle.value());
    handle_ = std::move(handle);
  }
  std::move(done_closure).Run();
}

void GetSettledFetchesTask::DidGetCompletedRequests(
    base::OnceClosure done_closure,
    const std::vector<std::string>& data,
    blink::ServiceWorkerStatusCode status) {
  switch (ToDatabaseStatus(status)) {
    case DatabaseStatus::kOk:
      break;
    case DatabaseStatus::kFailed:
      SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
      break;
    case DatabaseStatus::kNotFound:
      background_fetch_succeeded_ = false;
      error_ = blink::mojom::BackgroundFetchError::INVALID_ID;
      break;
  }

  completed_requests_.reserve(data.size());
  for (const std::string& serialized_completed_request : data) {
    completed_requests_.emplace_back();
    if (!completed_requests_.back().ParseFromString(
            serialized_completed_request)) {
      // Service worker database has been corrupted. Abandon fetches.
      SetStorageError(BackgroundFetchStorageError::kServiceWorkerStorageError);
      background_fetch_succeeded_ = false;
      AbandonFetches(registration_id_.service_worker_registration_id());
      break;
    }
    if (!completed_requests_.back().succeeded())
      background_fetch_succeeded_ = false;
  }
  std::move(done_closure).Run();
}

void GetSettledFetchesTask::GetResponses() {
  // Handle potential errors.
  if (HasStorageError()) {
    FinishWithError(blink::mojom::BackgroundFetchError::STORAGE_ERROR);
    return;
  }
  if (error_ != blink::mojom::BackgroundFetchError::NONE) {
    FinishWithError(error_);
    return;
  }
  if (completed_requests_.empty()) {
    FinishWithError(blink::mojom::BackgroundFetchError::NONE);
    return;
  }

  if (match_params_->FilterByRequest()) {
    // Get a response only for the relevant fetch.
    settled_fetches_.emplace_back();
    settled_fetches_.back().request = match_params_->request_to_match();
    for (const auto& completed_request : completed_requests_) {
      if (completed_request.serialized_request() !=
          match_params_->request_to_match().Serialize()) {
        continue;
      }
      // A matching request!
      FillResponse(&settled_fetches_.back(),
                   base::BindOnce(&GetSettledFetchesTask::FinishWithError,
                                  weak_factory_.GetWeakPtr(),
                                  blink::mojom::BackgroundFetchError::NONE));
      // TODO(crbug.com/863852): Add support for matchAll();
      return;
    }

    // No matching request found.
    FillUncachedResponse(
        &settled_fetches_.back(),
        base::BindOnce(&GetSettledFetchesTask::FinishWithError,
                       weak_factory_.GetWeakPtr(),
                       blink::mojom::BackgroundFetchError::NONE));
    return;
  }

  // Process all completed requests.
  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      completed_requests_.size(),
      base::BindOnce(&GetSettledFetchesTask::FinishWithError,
                     weak_factory_.GetWeakPtr(),
                     blink::mojom::BackgroundFetchError::NONE));
  settled_fetches_.reserve(completed_requests_.size());
  for (const auto& completed_request : completed_requests_) {
    settled_fetches_.emplace_back();
    settled_fetches_.back().request =
        std::move(ServiceWorkerFetchRequest::ParseFromString(
            completed_request.serialized_request()));
    FillResponse(&settled_fetches_.back(), barrier_closure);
  }
}

void GetSettledFetchesTask::FillResponse(
    BackgroundFetchSettledFetch* settled_fetch,
    base::OnceClosure callback) {
  DCHECK(settled_fetch);
  DCHECK(handle_.value());

  auto request =
      std::make_unique<ServiceWorkerFetchRequest>(settled_fetch->request);
  handle_.value()->Match(std::move(request),
                         match_params_->cloned_cache_query_params(),
                         base::BindOnce(&GetSettledFetchesTask::DidMatchRequest,
                                        weak_factory_.GetWeakPtr(),
                                        settled_fetch, std::move(callback)));
}

void GetSettledFetchesTask::DidMatchRequest(
    BackgroundFetchSettledFetch* settled_fetch,
    base::OnceClosure callback,
    blink::mojom::CacheStorageError error,
    blink::mojom::FetchAPIResponsePtr cache_response) {
  DCHECK(settled_fetch);

  // Handle error cases.
  if (error == blink::mojom::CacheStorageError::kErrorNotFound) {
    // If we are matching everything then we expect to find all responses
    // in the cache.
    if (!match_params_->FilterByRequest())
      SetStorageError(BackgroundFetchStorageError::kCacheStorageError);
  } else if (error != blink::mojom::CacheStorageError::kSuccess) {
    SetStorageError(BackgroundFetchStorageError::kCacheStorageError);
  }

  if (!cache_response) {
    FillUncachedResponse(settled_fetch, std::move(callback));
    return;
  }
  settled_fetch->response = std::move(cache_response);
  std::move(callback).Run();
}

void GetSettledFetchesTask::FillUncachedResponse(
    BackgroundFetchSettledFetch* settled_fetch,
    base::OnceClosure callback) {
  background_fetch_succeeded_ = false;

  // TODO(rayankans): Fill unmatched response with error reports.
  DCHECK(!settled_fetch->response);
  settled_fetch->response = blink::mojom::FetchAPIResponse::New();
  settled_fetch->response->response_type =
      network::mojom::FetchResponseType::kError;
  settled_fetch->response->url_list.push_back(settled_fetch->request.url);

  std::move(callback).Run();
}

void GetSettledFetchesTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  if (HasStorageError())
    error = blink::mojom::BackgroundFetchError::STORAGE_ERROR;
  ReportStorageError();
  std::move(settled_fetches_callback_)
      .Run(error, background_fetch_succeeded_, std::move(settled_fetches_),
           {} /* blob_data_handles */);
  Finished();  // Destroys |this|.
}

std::string GetSettledFetchesTask::HistogramName() const {
  return "GetSettledFetchesTask";
};

}  // namespace background_fetch

}  // namespace content
