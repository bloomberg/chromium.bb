// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "storage/browser/quota/quota_client.h"
#include "third_party/WebKit/common/quota/quota_types.mojom.h"

namespace content {
class CacheStorageManager;

// CacheStorageQuotaClient is owned by the QuotaManager. There is one per
// CacheStorageManager, and therefore one per
// ServiceWorkerContextCore.
class CONTENT_EXPORT CacheStorageQuotaClient : public storage::QuotaClient {
 public:
  explicit CacheStorageQuotaClient(
      base::WeakPtr<CacheStorageManager> cache_manager);
  ~CacheStorageQuotaClient() override;

  // QuotaClient overrides
  ID id() const override;
  void OnQuotaManagerDestroyed() override;
  void GetOriginUsage(const GURL& origin_url,
                      blink::mojom::StorageType type,
                      const GetUsageCallback& callback) override;
  void GetOriginsForType(blink::mojom::StorageType type,
                         const GetOriginsCallback& callback) override;
  void GetOriginsForHost(blink::mojom::StorageType type,
                         const std::string& host,
                         const GetOriginsCallback& callback) override;
  void DeleteOriginData(const GURL& origin,
                        blink::mojom::StorageType type,
                        const DeletionCallback& callback) override;
  bool DoesSupport(blink::mojom::StorageType type) const override;

 private:
  base::WeakPtr<CacheStorageManager> cache_manager_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageQuotaClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_QUOTA_CLIENT_H_
