// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/quota_dispatcher_host.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/url_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "url/origin.h"

using blink::mojom::StorageType;
using storage::QuotaClient;
using storage::QuotaManager;

namespace content {

// static
void QuotaDispatcherHost::Create(
    int process_id,
    QuotaManager* quota_manager,
    scoped_refptr<QuotaPermissionContext> permission_context,
    blink::mojom::QuotaDispatcherHostRequest request) {
  DCHECK(quota_manager);
  // TODO(sashab): Work out the conditions for permission_context to be set and
  // add a DCHECK for it here.
  mojo::MakeStrongBinding(
      std::make_unique<QuotaDispatcherHost>(process_id, quota_manager,
                                            std::move(permission_context)),
      std::move(request));
}

QuotaDispatcherHost::QuotaDispatcherHost(
    int process_id,
    QuotaManager* quota_manager,
    scoped_refptr<QuotaPermissionContext> permission_context)
    : process_id_(process_id),
      quota_manager_(quota_manager),
      permission_context_(std::move(permission_context)),
      weak_factory_(this) {}

void QuotaDispatcherHost::QueryStorageUsageAndQuota(
    const url::Origin& origin,
    StorageType storage_type,
    QueryStorageUsageAndQuotaCallback callback) {
  quota_manager_->GetUsageAndQuotaForWebApps(
      origin.GetURL(), storage_type,
      base::Bind(&QuotaDispatcherHost::DidQueryStorageUsageAndQuota,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(std::move(callback))));
}

void QuotaDispatcherHost::RequestStorageQuota(
    int64_t render_frame_id,
    const url::Origin& origin,
    StorageType storage_type,
    uint64_t requested_size,
    blink::mojom::QuotaDispatcherHost::RequestStorageQuotaCallback callback) {
  if (storage_type != StorageType::kTemporary &&
      storage_type != StorageType::kPersistent) {
    // Unsupported storage types.
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kErrorNotSupported,
                            0, 0);
    return;
  }

  DCHECK(storage_type == StorageType::kTemporary ||
         storage_type == StorageType::kPersistent);
  if (storage_type == StorageType::kPersistent) {
    quota_manager_->GetUsageAndQuotaForWebApps(
        origin.GetURL(), storage_type,
        base::Bind(&QuotaDispatcherHost::DidGetPersistentUsageAndQuota,
                   weak_factory_.GetWeakPtr(), render_frame_id, origin,
                   storage_type, requested_size,
                   base::Passed(std::move(callback))));
  } else {
    quota_manager_->GetUsageAndQuotaForWebApps(
        origin.GetURL(), storage_type,
        base::Bind(&QuotaDispatcherHost::DidGetTemporaryUsageAndQuota,
                   weak_factory_.GetWeakPtr(), requested_size,
                   base::Passed(std::move(callback))));
  }
}

void QuotaDispatcherHost::DidQueryStorageUsageAndQuota(
    RequestStorageQuotaCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  std::move(callback).Run(status, usage, quota);
}

void QuotaDispatcherHost::DidGetPersistentUsageAndQuota(
    int64_t render_frame_id,
    const url::Origin& origin,
    StorageType storage_type,
    uint64_t requested_quota,
    RequestStorageQuotaCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t current_usage,
    int64_t current_quota) {
  if (status != blink::mojom::QuotaStatusCode::kOk) {
    std::move(callback).Run(status, 0, 0);
    return;
  }

  // If we have enough quota for the requested storage, we can just let it go.
  // Convert the requested size from uint64_t to int64_t since the quota backend
  // requires int64_t values.
  // TODO(nhiroki): The backend should accept uint64_t values.
  int64_t requested_quota_signed =
      base::saturated_cast<int64_t>(requested_quota);
  if (quota_manager_->IsStorageUnlimited(origin.GetURL(), storage_type) ||
      requested_quota_signed <= current_quota) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, current_usage,
                            requested_quota);
    return;
  }

  // Otherwise we need to consult with the permission context and possibly show
  // a prompt.
  DCHECK(permission_context_);
  StorageQuotaParams params;
  params.render_frame_id = render_frame_id;
  params.origin_url = origin.GetURL();
  params.storage_type = storage_type;
  params.requested_size = requested_quota;

  permission_context_->RequestQuotaPermission(
      params, process_id_,
      base::Bind(&QuotaDispatcherHost::DidGetPermissionResponse,
                 weak_factory_.GetWeakPtr(), origin, requested_quota,
                 current_usage, current_quota,
                 base::Passed(std::move(callback))));
}

void QuotaDispatcherHost::DidGetPermissionResponse(
    const url::Origin& origin,
    uint64_t requested_quota,
    int64_t current_usage,
    int64_t current_quota,
    RequestStorageQuotaCallback callback,
    QuotaPermissionContext::QuotaPermissionResponse response) {
  // If user didn't allow the new quota, just return the current quota.
  if (response != QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, current_usage,
                            current_quota);
    return;
  }

  // Otherwise, return the new quota.
  // TODO(sashab): net::GetHostOrSpecFromURL(origin.GetURL()) potentially does
  // wasted work, e.g. if the origin has a host it can return that early. Maybe
  // rewrite to just convert the host to a string directly.
  quota_manager_->SetPersistentHostQuota(
      net::GetHostOrSpecFromURL(origin.GetURL()), requested_quota,
      base::Bind(&QuotaDispatcherHost::DidSetHostQuota,
                 weak_factory_.GetWeakPtr(), current_usage,
                 base::Passed(std::move(callback))));
}

void QuotaDispatcherHost::DidSetHostQuota(int64_t current_usage,
                                          RequestStorageQuotaCallback callback,
                                          blink::mojom::QuotaStatusCode status,
                                          int64_t new_quota) {
  std::move(callback).Run(status, current_usage, new_quota);
}

void QuotaDispatcherHost::DidGetTemporaryUsageAndQuota(
    int64_t requested_quota,
    RequestStorageQuotaCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  std::move(callback).Run(status, usage, std::min(requested_quota, quota));
}

QuotaDispatcherHost::~QuotaDispatcherHost() = default;

}  // namespace content
