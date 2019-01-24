// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/background_fetch/storage/get_request_blob_task.h"
#include "content/browser/background_fetch/background_fetch_request_match_params.h"
#include "content/browser/background_fetch/storage/database_helpers.h"
#include "content/common/background_fetch/background_fetch_types.h"

namespace content {
namespace background_fetch {

GetRequestBlobTask::GetRequestBlobTask(
    DatabaseTaskHost* host,
    const BackgroundFetchRegistrationId& registration_id,
    const scoped_refptr<BackgroundFetchRequestInfo>& request_info,
    GetRequestBlobCallback callback)
    : DatabaseTask(host),
      registration_id_(registration_id),
      request_info_(request_info),
      callback_(std::move(callback)),
      weak_factory_(this) {}

GetRequestBlobTask::~GetRequestBlobTask() = default;

void GetRequestBlobTask::Start() {
  CacheStorageHandle cache_storage = GetOrOpenCacheStorage(registration_id_);
  cache_storage.value()->OpenCache(
      /* cache_name= */ registration_id_.unique_id(),
      base::BindOnce(&GetRequestBlobTask::DidOpenCache,
                     weak_factory_.GetWeakPtr()));
}

void GetRequestBlobTask::DidOpenCache(CacheStorageCacheHandle handle,
                                      blink::mojom::CacheStorageError error) {
  if (error != blink::mojom::CacheStorageError::kSuccess) {
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  DCHECK(handle.value());
  auto request =
      BackgroundFetchSettledFetch::CloneRequest(request_info_->fetch_request());
  request->url = MakeCacheUrlUnique(request->url, registration_id_.unique_id(),
                                    request_info_->request_index());

  handle.value()->GetAllMatchedEntries(
      std::move(request), /* match_params= */ nullptr,
      base::BindOnce(&GetRequestBlobTask::DidMatchRequest,
                     weak_factory_.GetWeakPtr(), handle.Clone()));
}

void GetRequestBlobTask::DidMatchRequest(
    CacheStorageCacheHandle handle,
    blink::mojom::CacheStorageError error,
    std::vector<CacheStorageCache::CacheEntry> entries) {
  if (error != blink::mojom::CacheStorageError::kSuccess || entries.empty()) {
    SetStorageErrorAndFinish(BackgroundFetchStorageError::kCacheStorageError);
    return;
  }

  DCHECK_EQ(entries.size(), 1u);
  DCHECK(entries[0].first->blob);

  blob_ = std::move(entries[0].first->blob);
  FinishWithError(blink::mojom::BackgroundFetchError::NONE);
}

void GetRequestBlobTask::FinishWithError(
    blink::mojom::BackgroundFetchError error) {
  ReportStorageError();

  std::move(callback_).Run(error, std::move(blob_));
  Finished();
}

std::string GetRequestBlobTask::HistogramName() const {
  return "GetRequestBlobTask";
}

}  // namespace background_fetch
}  // namespace content
