// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/usage_and_quota_query.h"

#include "net/base/net_util.h"

namespace content {
namespace devtools {
namespace page {

UsageAndQuotaQuery::UsageAndQuotaQuery(
    scoped_refptr<storage::QuotaManager> quota_manager,
    const GURL& security_origin,
    const Callback& callback)
    : quota_manager_(quota_manager),
      security_origin_(security_origin),
      callback_(callback),
      temporary_quota_(0.0),
      persistent_quota_(0.0) {
  AddRef();
  quota_manager->GetUsageAndQuotaForWebApps(
      security_origin,
      storage::kStorageTypeTemporary,
      base::Bind(&UsageAndQuotaQuery::DidGetTemporaryQuota, this));
  quota_manager->GetPersistentHostQuota(
      net::GetHostOrSpecFromURL(security_origin),
      base::Bind(&UsageAndQuotaQuery::DidGetPersistentQuota, this));
  GetHostUsage(&temporary_usage_, storage::kStorageTypeTemporary);
  GetHostUsage(&persistent_usage_, storage::kStorageTypePersistent);
  GetHostUsage(&syncable_usage_, storage::kStorageTypeSyncable);
  Release();
}

UsageAndQuotaQuery::~UsageAndQuotaQuery() {
  callback_.Run(QueryUsageAndQuotaResponse::Create()
      ->set_quota(Quota::Create()->set_temporary(temporary_quota_)
                                 ->set_persistent(persistent_quota_))
      ->set_usage(Usage::Create()->set_temporary(temporary_usage_)
                                 ->set_persistent(persistent_usage_)
                                 ->set_syncable(syncable_usage_)));
}

void UsageAndQuotaQuery::DidGetTemporaryQuota(storage::QuotaStatusCode status,
                                              int64 used_bytes,
                                              int64 quota_in_bytes) {
  if (status == storage::kQuotaStatusOk)
    temporary_quota_ = quota_in_bytes;
}

void UsageAndQuotaQuery::DidGetPersistentQuota(storage::QuotaStatusCode status,
                                               int64 value) {
  if (status == storage::kQuotaStatusOk)
    persistent_quota_ = value;
}

void UsageAndQuotaQuery::GetHostUsage(UsageItems* list,
                                      storage::StorageType storage_type) {
  GetUsageForClient(list, storage_type, storage::QuotaClient::kFileSystem,
                    usage_item::kIdFilesystem);
  GetUsageForClient(list, storage_type, storage::QuotaClient::kDatabase,
                    usage_item::kIdDatabase);
  GetUsageForClient(list, storage_type, storage::QuotaClient::kAppcache,
                    usage_item::kIdAppcache);
  GetUsageForClient(list, storage_type, storage::QuotaClient::kIndexedDatabase,
                    usage_item::kIdIndexeddatabase);
}

void UsageAndQuotaQuery::GetUsageForClient(UsageItems* list,
                                           storage::StorageType storage_type,
                                           storage::QuotaClient::ID client_id,
                                           const std::string& client_name) {
  if (!quota_manager_->IsTrackingHostUsage(storage_type, client_id))
    return;
  quota_manager_->GetHostUsage(
      net::GetHostOrSpecFromURL(security_origin_),
      storage_type,
      client_id,
      base::Bind(&UsageAndQuotaQuery::DidGetUsageForClient,
                 this, list, client_name));
}

void UsageAndQuotaQuery::DidGetUsageForClient(UsageItems* list,
                                              const std::string& client_name,
                                              int64 value) {
  list->push_back(UsageItem::Create()->set_id(client_name)->set_value(value));
}

}  // namespace page
}  // namespace devtools
}  // namespace content
