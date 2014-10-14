// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/usage_and_quota_query.h"

#include "net/base/net_util.h"

namespace content {
namespace devtools {
namespace page {

namespace {

class UsageQuery : public base::RefCounted<UsageQuery> {
 public:
  using Callback = base::Callback<void(const std::vector<UsageItem>&)>;

  UsageQuery(scoped_refptr<storage::QuotaManager> quota_manager,
             const std::string& host,
             storage::StorageType storage_type,
             const Callback& callback)
             : quota_manager_(quota_manager),
               host_(host),
               storage_type_(storage_type),
               callback_(callback) {
    AddRef();
    GetForClient(storage::QuotaClient::kFileSystem,
                 usage_item::kIdFilesystem);
    GetForClient(storage::QuotaClient::kDatabase,
                 usage_item::kIdDatabase);
    GetForClient(storage::QuotaClient::kAppcache,
                 usage_item::kIdAppcache);
    GetForClient(storage::QuotaClient::kIndexedDatabase,
                 usage_item::kIdIndexeddatabase);
    Release();
  }

 private:
  friend class base::RefCounted<UsageQuery>;

  ~UsageQuery() {
    callback_.Run(usage_list_);
  }

  void GetForClient(storage::QuotaClient::ID client_id,
                    const std::string& client_name) {
    if (!quota_manager_->IsTrackingHostUsage(storage_type_, client_id))
      return;
    quota_manager_->GetHostUsage(
        host_, storage_type_, client_id,
        base::Bind(&UsageQuery::DidGetForClient, this, client_name));
  }

  void DidGetForClient(const std::string& client_name, int64 value) {
    UsageItem usage_item;
    usage_item.set_id(client_name);
    usage_item.set_value(value);
    usage_list_.push_back(usage_item);
  }

  scoped_refptr<storage::QuotaManager> quota_manager_;
  std::string host_;
  storage::StorageType storage_type_;
  std::vector<UsageItem> usage_list_;
  Callback callback_;
};

}  // namespace

UsageAndQuotaQuery::UsageAndQuotaQuery(
    scoped_refptr<storage::QuotaManager> quota_manager,
    const GURL& security_origin,
    const Callback& callback)
    : quota_manager_(quota_manager),
      security_origin_(security_origin),
      callback_(callback) {
  AddRef();
  quota_manager->GetUsageAndQuotaForWebApps(
      security_origin,
      storage::kStorageTypeTemporary,
      base::Bind(&UsageAndQuotaQuery::DidGetTemporaryQuota, this));
  quota_manager->GetPersistentHostQuota(
      net::GetHostOrSpecFromURL(security_origin),
      base::Bind(&UsageAndQuotaQuery::DidGetPersistentQuota, this));
  GetHostUsage(storage::kStorageTypeTemporary,
               base::Bind(&Usage::set_temporary, base::Unretained(&usage_)));
  GetHostUsage(storage::kStorageTypePersistent,
               base::Bind(&Usage::set_persistent, base::Unretained(&usage_)));
  GetHostUsage(storage::kStorageTypeSyncable,
               base::Bind(&Usage::set_syncable, base::Unretained(&usage_)));
  Release();
}

UsageAndQuotaQuery::~UsageAndQuotaQuery() {
  scoped_ptr<QueryUsageAndQuotaResponse> response(
      new QueryUsageAndQuotaResponse);
  response->set_quota(quota_);
  response->set_usage(usage_);
  callback_.Run(response.Pass());
}

void UsageAndQuotaQuery::DidGetTemporaryQuota(storage::QuotaStatusCode status,
                                              int64 used_bytes,
                                              int64 quota_in_bytes) {
  if (status == storage::kQuotaStatusOk)
    quota_.set_temporary(quota_in_bytes);
}

void UsageAndQuotaQuery::DidGetPersistentQuota(storage::QuotaStatusCode status,
                                               int64 value) {
  if (status == storage::kQuotaStatusOk)
    quota_.set_persistent(value);
}

void UsageAndQuotaQuery::GetHostUsage(
    storage::StorageType storage_type,
    const UsageItemsCallback& items_callback) {
  // |base::Bind| is used instead of passing |items_callback| directly
  // so that |this| is retained.
  new UsageQuery(quota_manager_,
                 net::GetHostOrSpecFromURL(security_origin_),
                 storage_type,
                 base::Bind(&UsageAndQuotaQuery::DidGetHostUsage,
                            this, items_callback));
}

void UsageAndQuotaQuery::DidGetHostUsage(
    const UsageItemsCallback& items_callback,
    const std::vector<UsageItem>& usage_list) {
  items_callback.Run(usage_list);
}

}  // namespace page
}  // namespace devtools
}  // namespace content
