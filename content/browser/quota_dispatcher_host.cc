// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/quota_dispatcher_host.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/quota_permission_context.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/url_util.h"
#include "storage/browser/quota/quota_manager.h"
#include "url/gurl.h"

using storage::QuotaClient;
using storage::QuotaManager;
using storage::QuotaStatusCode;
using storage::StorageType;

namespace content {

// Created one per request to carry the request's request_id around.
// Dispatches requests from renderer/worker to the QuotaManager and
// sends back the response to the renderer/worker.
// TODO(sashab): Remove, since request_id is no longer needed.
class QuotaDispatcherHost::RequestDispatcher {
 public:
  RequestDispatcher(base::WeakPtr<QuotaDispatcherHost> dispatcher_host)
      : dispatcher_host_(dispatcher_host),
        render_process_id_(dispatcher_host->process_id_) {
    request_id_ =
        dispatcher_host_->outstanding_requests_.Add(base::WrapUnique(this));
  }
  virtual ~RequestDispatcher() {}

 protected:
  // Subclass must call this when it's done with the request.
  void Completed() {
    if (dispatcher_host_)
      dispatcher_host_->outstanding_requests_.Remove(request_id_);
  }

  QuotaDispatcherHost* dispatcher_host() const {
    return dispatcher_host_.get();
  }
  storage::QuotaManager* quota_manager() const {
    return dispatcher_host_ ? dispatcher_host_->quota_manager_ : nullptr;
  }
  QuotaPermissionContext* permission_context() const {
    return dispatcher_host_ ? dispatcher_host_->permission_context_.get()
                            : nullptr;
  }
  int render_process_id() const { return render_process_id_; }

 private:
  base::WeakPtr<QuotaDispatcherHost> dispatcher_host_;
  int render_process_id_;
  int request_id_;
};

class QuotaDispatcherHost::QueryUsageAndQuotaDispatcher
    : public RequestDispatcher {
 public:
  QueryUsageAndQuotaDispatcher(
      base::WeakPtr<QuotaDispatcherHost> dispatcher_host,
      QueryStorageUsageAndQuotaCallback callback)
      : RequestDispatcher(dispatcher_host),
        callback_(std::move(callback)),
        weak_factory_(this) {}
  ~QueryUsageAndQuotaDispatcher() override {}

  void QueryStorageUsageAndQuota(const GURL& origin, StorageType type) {
    // crbug.com/349708
    TRACE_EVENT0("io",
                 "QuotaDispatcherHost::QueryUsageAndQuotaDispatcher"
                 "::QueryStorageUsageAndQuota");

    quota_manager()->GetUsageAndQuotaForWebApps(
        origin, type,
        base::Bind(&QueryUsageAndQuotaDispatcher::DidQueryStorageUsageAndQuota,
                   weak_factory_.GetWeakPtr()));
  }

 private:
  void DidQueryStorageUsageAndQuota(QuotaStatusCode status,
                                    int64_t usage,
                                    int64_t quota) {
    if (!dispatcher_host())
      return;
    // crbug.com/349708
    TRACE_EVENT0("io", "QuotaDispatcherHost::RequestQuotaDispatcher"
                 "::DidQueryStorageUsageAndQuota");
    std::move(callback_).Run(status, usage, quota);
    Completed();
  }

  QueryStorageUsageAndQuotaCallback callback_;
  base::WeakPtrFactory<QueryUsageAndQuotaDispatcher> weak_factory_;
};

class QuotaDispatcherHost::RequestQuotaDispatcher
    : public RequestDispatcher {
 public:
  typedef RequestQuotaDispatcher self_type;

  RequestQuotaDispatcher(
      base::WeakPtr<QuotaDispatcherHost> dispatcher_host,
      int64_t render_frame_id,
      const GURL& origin_url,
      storage::StorageType storage_type,
      uint64_t requested_size,
      mojom::QuotaDispatcherHost::RequestStorageQuotaCallback callback)
      : RequestDispatcher(dispatcher_host),
        render_frame_id_(render_frame_id),
        origin_url_(origin_url),
        storage_type_(storage_type),
        requested_size_(requested_size),
        callback_(std::move(callback)),
        current_usage_(0),
        current_quota_(0),
        requested_quota_(0),
        weak_factory_(this) {
    // Convert the requested size from uint64_t to int64_t since the quota
    // backend requires int64_t values.
    // TODO(nhiroki): The backend should accept uint64_t values.
    requested_quota_ = base::saturated_cast<int64_t>(requested_size);
  }
  ~RequestQuotaDispatcher() override {}

  void Start() {
    DCHECK(dispatcher_host());
    // crbug.com/349708
    TRACE_EVENT0("io", "QuotaDispatcherHost::RequestQuotaDispatcher::Start");

    DCHECK(storage_type_ == storage::kStorageTypeTemporary ||
           storage_type_ == storage::kStorageTypePersistent);
    if (storage_type_ == storage::kStorageTypePersistent) {
      quota_manager()->GetUsageAndQuotaForWebApps(
          origin_url_, storage_type_,
          base::Bind(&self_type::DidGetPersistentUsageAndQuota,
                     weak_factory_.GetWeakPtr()));
    } else {
      quota_manager()->GetUsageAndQuotaForWebApps(
          origin_url_, storage_type_,
          base::Bind(&self_type::DidGetTemporaryUsageAndQuota,
                     weak_factory_.GetWeakPtr()));
    }
  }

 private:
  void DidGetPersistentUsageAndQuota(QuotaStatusCode status,
                                     int64_t usage,
                                     int64_t quota) {
    if (!dispatcher_host())
      return;
    if (status != storage::kQuotaStatusOk) {
      DidFinish(status, 0, 0);
      return;
    }

    if (quota_manager()->IsStorageUnlimited(origin_url_, storage_type_) ||
        requested_quota_ <= quota) {
      // Seems like we can just let it go.
      DidFinish(storage::kQuotaStatusOk, usage, requested_size_);
      return;
    }
    current_usage_ = usage;
    current_quota_ = quota;

    // Otherwise we need to consult with the permission context and
    // possibly show a prompt.
    DCHECK(permission_context());

    StorageQuotaParams params;
    params.render_frame_id = render_frame_id_;
    params.origin_url = origin_url_;
    params.storage_type = storage_type_;
    params.requested_size = requested_size_;

    permission_context()->RequestQuotaPermission(
        params, render_process_id(),
        base::Bind(&self_type::DidGetPermissionResponse,
                   weak_factory_.GetWeakPtr()));
  }

  void DidGetTemporaryUsageAndQuota(QuotaStatusCode status,
                                    int64_t usage,
                                    int64_t quota) {
    DidFinish(status, usage, std::min(requested_quota_, quota));
  }

  void DidGetPermissionResponse(
      QuotaPermissionContext::QuotaPermissionResponse response) {
    if (!dispatcher_host())
      return;
    if (response != QuotaPermissionContext::QUOTA_PERMISSION_RESPONSE_ALLOW) {
      // User didn't allow the new quota.  Just returning the current quota.
      DidFinish(storage::kQuotaStatusOk, current_usage_, current_quota_);
      return;
    }
    // Now we're allowed to set the new quota.
    quota_manager()->SetPersistentHostQuota(
        net::GetHostOrSpecFromURL(origin_url_), requested_size_,
        base::Bind(&self_type::DidSetHostQuota, weak_factory_.GetWeakPtr()));
  }

  void DidSetHostQuota(QuotaStatusCode status, int64_t new_quota) {
    DidFinish(status, current_usage_, new_quota);
  }

  void DidFinish(QuotaStatusCode status, int64_t usage, int64_t granted_quota) {
    if (!dispatcher_host())
      return;
    DCHECK(dispatcher_host());
    std::move(callback_).Run(status, usage, granted_quota);
    Completed();
  }

  const int64_t render_frame_id_;
  const GURL origin_url_;
  const storage::StorageType storage_type_;
  const uint64_t requested_size_;
  mojom::QuotaDispatcherHost::RequestStorageQuotaCallback callback_;

  int64_t current_usage_;
  int64_t current_quota_;
  int64_t requested_quota_;
  base::WeakPtrFactory<self_type> weak_factory_;
};

// static
void QuotaDispatcherHost::Create(
    int process_id,
    QuotaManager* quota_manager,
    scoped_refptr<QuotaPermissionContext> permission_context,
    mojom::QuotaDispatcherHostRequest request) {
  mojo::MakeStrongBinding(
      base::MakeUnique<QuotaDispatcherHost>(process_id, quota_manager,
                                            std::move(permission_context)),
      std::move(request));
}

QuotaDispatcherHost::QuotaDispatcherHost(
    int process_id,
    QuotaManager* quota_manager,
    scoped_refptr<QuotaPermissionContext> permission_context)
    : process_id_(process_id),
      quota_manager_(quota_manager),
      permission_context_(permission_context),
      weak_factory_(this) {}

void QuotaDispatcherHost::QueryStorageUsageAndQuota(
    const GURL& origin_url,
    storage::StorageType storage_type,
    QueryStorageUsageAndQuotaCallback callback) {
  QueryUsageAndQuotaDispatcher* dispatcher = new QueryUsageAndQuotaDispatcher(
      weak_factory_.GetWeakPtr(), std::move(callback));
  dispatcher->QueryStorageUsageAndQuota(origin_url, storage_type);
}

void QuotaDispatcherHost::RequestStorageQuota(
    int64_t render_frame_id,
    const GURL& origin_url,
    storage::StorageType storage_type,
    uint64_t requested_size,
    mojom::QuotaDispatcherHost::RequestStorageQuotaCallback callback) {
  if (storage_type != storage::kStorageTypeTemporary &&
      storage_type != storage::kStorageTypePersistent) {
    // Unsupported storage types.
    std::move(callback).Run(storage::kQuotaErrorNotSupported, 0, 0);
    return;
  }

  RequestQuotaDispatcher* dispatcher = new RequestQuotaDispatcher(
      weak_factory_.GetWeakPtr(), render_frame_id, origin_url, storage_type,
      requested_size, std::move(callback));
  dispatcher->Start();
}

QuotaDispatcherHost::~QuotaDispatcherHost() = default;

}  // namespace content
