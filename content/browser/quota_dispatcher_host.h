// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_QUOTA_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_QUOTA_DISPATCHER_HOST_H_

#include "base/macros.h"
#include "content/common/quota_dispatcher_host.mojom.h"
#include "content/public/browser/quota_permission_context.h"
#include "storage/common/quota/quota_status_code.h"

class GURL;

namespace storage {
class QuotaManager;
}

namespace content {
class QuotaPermissionContext;

class QuotaDispatcherHost : public mojom::QuotaDispatcherHost {
 public:
  static void Create(int process_id,
                     storage::QuotaManager* quota_manager,
                     scoped_refptr<QuotaPermissionContext> permission_context,
                     mojom::QuotaDispatcherHostRequest request);

  QuotaDispatcherHost(int process_id,
                      storage::QuotaManager* quota_manager,
                      scoped_refptr<QuotaPermissionContext> permission_context);

  ~QuotaDispatcherHost() override;

  // content::mojom::QuotaDispatcherHost:
  void QueryStorageUsageAndQuota(
      const GURL& origin_url,
      storage::StorageType storage_type,
      QueryStorageUsageAndQuotaCallback callback) override;
  void RequestStorageQuota(int64_t render_frame_id,
                           const GURL& origin_url,
                           storage::StorageType storage_type,
                           uint64_t requested_size,
                           RequestStorageQuotaCallback callback) override;

 private:
  void DidQueryStorageUsageAndQuota(RequestStorageQuotaCallback callback,
                                    storage::QuotaStatusCode status,
                                    int64_t usage,
                                    int64_t quota);
  void DidGetPersistentUsageAndQuota(int64_t render_frame_id,
                                     const GURL& origin_url,
                                     storage::StorageType storage_type,
                                     uint64_t requested_quota,
                                     RequestStorageQuotaCallback callback,
                                     storage::QuotaStatusCode status,
                                     int64_t usage,
                                     int64_t quota);
  void DidGetPermissionResponse(
      const GURL& origin_url,
      uint64_t requested_quota,
      int64_t current_usage,
      int64_t current_quota,
      RequestStorageQuotaCallback callback,
      QuotaPermissionContext::QuotaPermissionResponse response);
  void DidSetHostQuota(int64_t current_usage,
                       RequestStorageQuotaCallback callback,
                       storage::QuotaStatusCode status,
                       int64_t new_quota);
  void DidGetTemporaryUsageAndQuota(int64_t requested_quota,
                                    RequestStorageQuotaCallback callback,
                                    storage::QuotaStatusCode status,
                                    int64_t usage,
                                    int64_t quota);

  // The ID of this process.
  int process_id_;

  storage::QuotaManager* quota_manager_;
  scoped_refptr<QuotaPermissionContext> permission_context_;

  base::WeakPtrFactory<QuotaDispatcherHost> weak_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(QuotaDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_QUOTA_DISPATCHER_HOST_H_
