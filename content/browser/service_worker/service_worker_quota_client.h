// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_QUOTA_CLIENT_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_QUOTA_CLIENT_H_

#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/common/quota/quota_types.h"

namespace content {
class ServiceWorkerContextWrapper;

class ServiceWorkerQuotaClient : public storage::QuotaClient {
 public:
  virtual ~ServiceWorkerQuotaClient();

  // QuotaClient method overrides
  virtual ID id() const override;
  virtual void OnQuotaManagerDestroyed() override;
  virtual void GetOriginUsage(const GURL& origin,
                              storage::StorageType type,
                              const GetUsageCallback& callback) override;
  virtual void GetOriginsForType(storage::StorageType type,
                                 const GetOriginsCallback& callback) override;
  virtual void GetOriginsForHost(storage::StorageType type,
                                 const std::string& host,
                                 const GetOriginsCallback& callback) override;
  virtual void DeleteOriginData(const GURL& origin,
                                storage::StorageType type,
                                const DeletionCallback& callback) override;
  virtual bool DoesSupport(storage::StorageType type) const override;

 private:
  friend class ServiceWorkerContextWrapper;
  friend class ServiceWorkerQuotaClientTest;

  CONTENT_EXPORT explicit ServiceWorkerQuotaClient(
      ServiceWorkerContextWrapper* context);

  scoped_refptr<ServiceWorkerContextWrapper> context_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerQuotaClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_QUOTA_CLIENT_H_
