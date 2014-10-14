// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_QUOTA_AND_USAGE_QUERY_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_QUOTA_AND_USAGE_QUERY_H_

#include "content/browser/devtools/protocol/devtools_protocol_handler_impl.h"
#include "storage/browser/quota/quota_manager.h"

namespace content {
namespace devtools {
namespace page {

// This class can only be used on IO thread.
class UsageAndQuotaQuery : public base::RefCounted<UsageAndQuotaQuery> {
 public:
  using Callback = base::Callback<void(scoped_ptr<QueryUsageAndQuotaResponse>)>;

  UsageAndQuotaQuery(scoped_refptr<storage::QuotaManager> quota_manager,
                     const GURL& security_origin,
                     const Callback& callback);

 private:
  friend class base::RefCounted<UsageAndQuotaQuery>;

  virtual ~UsageAndQuotaQuery();

  void DidGetTemporaryQuota(storage::QuotaStatusCode status,
                            int64 used_bytes,
                            int64 quota_in_bytes);

  void DidGetPersistentQuota(storage::QuotaStatusCode status, int64 value);

  using UsageItemsCallback =
      base::Callback<void(const std::vector<UsageItem>&)>;

  void GetHostUsage(storage::StorageType storage_type,
                    const UsageItemsCallback& items_callback);

  void DidGetHostUsage(const UsageItemsCallback& items_callback,
                       const std::vector<UsageItem>& usage_list);

  scoped_refptr<storage::QuotaManager> quota_manager_;
  GURL security_origin_;
  Callback callback_;
  Quota quota_;
  Usage usage_;
};

}  // namespace page
}  // namespace devtools
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_QUOTA_AND_USAGE_QUERY_H_
