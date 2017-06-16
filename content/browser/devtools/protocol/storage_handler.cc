// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/storage_handler.h"

#include <unordered_set>
#include <utility>
#include <vector>

#include "base/strings/string_split.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/quota/quota_manager.h"
#include "storage/common/quota/quota_status_code.h"

namespace content {
namespace protocol {

namespace {
static const char kAppCache[] = "appcache";
static const char kCookies[] = "cookies";
static const char kFileSystems[] = "filesystems";
static const char kIndexedDB[] = "indexeddb";
static const char kLocalStorage[] = "local_storage";
static const char kShaderCache[] = "shader_cache";
static const char kWebSQL[] = "websql";
static const char kServiceWorkers[] = "service_workers";
static const char kCacheStorage[] = "cache_storage";
static const char kAll[] = "all";

void ReportUsageAndQuotaDataOnUIThread(
    std::unique_ptr<StorageHandler::GetUsageAndQuotaCallback> callback,
    storage::QuotaStatusCode code,
    int64_t usage,
    int64_t quota) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (code != storage::kQuotaStatusOk) {
    return callback->sendFailure(
        Response::Error("Quota information is not available"));
  }
  callback->sendSuccess(usage, quota);
}

void GotUsageAndQuotaDataCallback(
    std::unique_ptr<StorageHandler::GetUsageAndQuotaCallback> callback,
    storage::QuotaStatusCode code,
    int64_t usage,
    int64_t quota) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(ReportUsageAndQuotaDataOnUIThread,
                 base::Passed(std::move(callback)), code, usage, quota));
}

void GetUsageAndQuotaOnIOThread(
    storage::QuotaManager* manager,
    const GURL& url,
    std::unique_ptr<StorageHandler::GetUsageAndQuotaCallback> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  manager->GetUsageAndQuotaForWebApps(
      url, storage::kStorageTypeTemporary,
      base::Bind(&GotUsageAndQuotaDataCallback,
                 base::Passed(std::move(callback))));
}
}  // namespace

StorageHandler::StorageHandler()
    : DevToolsDomainHandler(Storage::Metainfo::domainName),
      host_(nullptr) {
}

StorageHandler::~StorageHandler() = default;

void StorageHandler::Wire(UberDispatcher* dispatcher) {
  Storage::Dispatcher::wire(dispatcher, this);
}

void StorageHandler::SetRenderFrameHost(RenderFrameHostImpl* host) {
  host_ = host;
}

Response StorageHandler::ClearDataForOrigin(
    const std::string& origin,
    const std::string& storage_types) {
  if (!host_)
    return Response::InternalError();

  StoragePartition* partition = host_->GetProcess()->GetStoragePartition();
  std::vector<std::string> types = base::SplitString(
      storage_types, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::unordered_set<std::string> set(types.begin(), types.end());
  uint32_t remove_mask = 0;
  if (set.count(kAppCache))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_APPCACHE;
  if (set.count(kCookies))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_COOKIES;
  if (set.count(kFileSystems))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS;
  if (set.count(kIndexedDB))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_INDEXEDDB;
  if (set.count(kLocalStorage))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_LOCAL_STORAGE;
  if (set.count(kShaderCache))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SHADER_CACHE;
  if (set.count(kWebSQL))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_WEBSQL;
  if (set.count(kServiceWorkers))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS;
  if (set.count(kCacheStorage))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE;
  if (set.count(kAll))
    remove_mask |= StoragePartition::REMOVE_DATA_MASK_ALL;

  if (!remove_mask)
    return Response::InvalidParams("No valid storage type specified");

  partition->ClearDataForOrigin(
      remove_mask,
      StoragePartition::QUOTA_MANAGED_STORAGE_MASK_ALL,
      GURL(origin),
      partition->GetURLRequestContext(),
      base::Bind(&base::DoNothing));
  return Response::OK();
}

void StorageHandler::GetUsageAndQuota(
    const String& origin,
    std::unique_ptr<GetUsageAndQuotaCallback> callback) {
  if (!host_)
    return callback->sendFailure(Response::InternalError());

  GURL origin_url(origin);
  if (!origin_url.is_valid()) {
    return callback->sendFailure(
        Response::Error(origin + " is not a valid URL"));
  }

  storage::QuotaManager* manager =
      host_->GetProcess()->GetStoragePartition()->GetQuotaManager();
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetUsageAndQuotaOnIOThread, manager, origin_url,
                 base::Passed(std::move(callback))));
}

}  // namespace protocol
}  // namespace content
