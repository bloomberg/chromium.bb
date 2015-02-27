// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_USAGE_AND_QUOTA_QUERY_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_USAGE_AND_QUOTA_QUERY_H_

#include "content/browser/devtools/protocol/devtools_protocol_handler.h"
#include "storage/browser/quota/quota_manager.h"

namespace content {
namespace devtools {
namespace page {

// This class can only be used on IO thread.
class UsageAndQuotaQuery : public base::RefCounted<UsageAndQuotaQuery> {
 public:
  using Callback =
      base::Callback<void(scoped_refptr<QueryUsageAndQuotaResponse>)>;

  UsageAndQuotaQuery(scoped_refptr<storage::QuotaManager> quota_manager,
                     const GURL& security_origin,
                     const Callback& callback);

 private:
  friend class base::RefCounted<UsageAndQuotaQuery>;

  using UsageItems = std::vector<scoped_refptr<UsageItem>>;

  virtual ~UsageAndQuotaQuery();

  void DidGetTemporaryQuota(storage::QuotaStatusCode status,
                            int64 used_bytes,
                            int64 quota_in_bytes);

  void DidGetPersistentQuota(storage::QuotaStatusCode status, int64 value);

  void GetHostUsage(UsageItems* list, storage::StorageType storage_type);

  void GetUsageForClient(UsageItems* list,
                         storage::StorageType storage_type,
                         storage::QuotaClient::ID client_id,
                         const std::string& client_name);

  void DidGetUsageForClient(UsageItems* list,
                            const std::string& client_name,
                            int64 value);

  scoped_refptr<storage::QuotaManager> quota_manager_;
  GURL security_origin_;
  Callback callback_;
  double temporary_quota_;
  double persistent_quota_;
  UsageItems temporary_usage_;
  UsageItems persistent_usage_;
  UsageItems syncable_usage_;
};

}  // namespace page
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_USAGE_AND_QUOTA_QUERY_H_
