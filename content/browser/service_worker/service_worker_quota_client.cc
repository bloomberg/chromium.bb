// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/browser/service_worker/service_worker_quota_client.h"

#include "base/bind.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"

using blink::mojom::StorageType;
using storage::QuotaClient;

namespace content {
namespace {
void ReportOrigins(const QuotaClient::GetOriginsCallback& callback,
                   bool restrict_on_host,
                   const std::string host,
                   const std::vector<ServiceWorkerUsageInfo>& usage_info) {
  std::set<GURL> origins;
  for (const ServiceWorkerUsageInfo& info : usage_info) {
    if (restrict_on_host && info.origin.host() != host) {
      continue;
    }
    origins.insert(info.origin);
  }
  callback.Run(origins);
}

void ReportToQuotaStatus(const QuotaClient::DeletionCallback& callback,
                         bool status) {
  callback.Run(status ? blink::mojom::QuotaStatusCode::kOk
                      : blink::mojom::QuotaStatusCode::kUnknown);
}

void FindUsageForOrigin(const QuotaClient::GetUsageCallback& callback,
                        const GURL& origin,
                        const std::vector<ServiceWorkerUsageInfo>& usage_info) {
  for (const auto& info : usage_info) {
    if (info.origin == origin) {
      callback.Run(info.total_size_bytes);
      return;
    }
  }
  callback.Run(0);
}
}  // namespace

ServiceWorkerQuotaClient::ServiceWorkerQuotaClient(
    ServiceWorkerContextWrapper* context)
    : context_(context) {
}

ServiceWorkerQuotaClient::~ServiceWorkerQuotaClient() {
}

QuotaClient::ID ServiceWorkerQuotaClient::id() const {
  return QuotaClient::kServiceWorker;
}

void ServiceWorkerQuotaClient::OnQuotaManagerDestroyed() {
  delete this;
}

void ServiceWorkerQuotaClient::GetOriginUsage(
    const GURL& origin,
    StorageType type,
    const GetUsageCallback& callback) {
  if (type != StorageType::kTemporary) {
    callback.Run(0);
    return;
  }
  context_->GetAllOriginsInfo(
      base::Bind(&FindUsageForOrigin, callback, origin));
}

void ServiceWorkerQuotaClient::GetOriginsForType(
    StorageType type,
    const GetOriginsCallback& callback) {
  if (type != StorageType::kTemporary) {
    callback.Run(std::set<GURL>());
    return;
  }
  context_->GetAllOriginsInfo(base::Bind(&ReportOrigins, callback, false, ""));
}

void ServiceWorkerQuotaClient::GetOriginsForHost(
    StorageType type,
    const std::string& host,
    const GetOriginsCallback& callback) {
  if (type != StorageType::kTemporary) {
    callback.Run(std::set<GURL>());
    return;
  }
  context_->GetAllOriginsInfo(base::Bind(&ReportOrigins, callback, true, host));
}

void ServiceWorkerQuotaClient::DeleteOriginData(
    const GURL& origin,
    StorageType type,
    const DeletionCallback& callback) {
  if (type != StorageType::kTemporary) {
    callback.Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }
  context_->DeleteForOrigin(origin, base::Bind(&ReportToQuotaStatus, callback));
}

bool ServiceWorkerQuotaClient::DoesSupport(StorageType type) const {
  return type == StorageType::kTemporary;
}

}  // namespace content
