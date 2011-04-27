// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/quota_dispatcher_host.h"

#include "base/memory/scoped_callback_factory.h"
#include "content/common/quota_messages.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/quota_manager.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageQuotaError.h"

using quota::QuotaManager;
using quota::QuotaStatusCode;
using quota::StorageType;
using WebKit::WebStorageQuotaError;

// Created one per request to carry the request's request_id around.
// Dispatches requests from renderer/worker to the QuotaManager and
// sends back the response to the renderer/worker.
class QuotaDispatcherHost::RequestDispatcher {
 public:
  RequestDispatcher(QuotaDispatcherHost* dispatcher_host,
                    quota::QuotaManager* quota_manager,
                    int request_id)
      : dispatcher_host_(dispatcher_host),
        quota_manager_(quota_manager),
        request_id_(request_id),
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
    DCHECK(quota_manager_);
    dispatcher_host_->outstanding_requests_.AddWithID(this, request_id_);
  }

  ~RequestDispatcher() {
  }

  void QueryStorageUsageAndQuota(const GURL& origin, StorageType type) {
    quota_manager_->GetUsageAndQuota(origin, type,
        callback_factory_.NewCallback(
            &RequestDispatcher::DidQueryStorageUsageAndQuota));
  }

  void RequestStorageQuota(const GURL& origin, StorageType type,
                           int64 requested_size) {
    quota_manager_->RequestQuota(origin, type, requested_size,
        callback_factory_.NewCallback(
            &RequestDispatcher::DidRequestStorageQuota));
  }

 private:
  void DidQueryStorageUsageAndQuota(
      QuotaStatusCode status, int64 usage, int64 quota) {
    DCHECK(dispatcher_host_);
    if (status != quota::kQuotaStatusOk) {
      dispatcher_host_->Send(new QuotaMsg_DidFail(
          request_id_, static_cast<WebStorageQuotaError>(status)));
    } else {
      dispatcher_host_->Send(new QuotaMsg_DidQueryStorageUsageAndQuota(
          request_id_, usage, quota));
    }
    dispatcher_host_->outstanding_requests_.Remove(request_id_);
  }

  void DidRequestStorageQuota(QuotaStatusCode status, int64 granted_quota) {
    DCHECK(dispatcher_host_);
    if (status != quota::kQuotaStatusOk) {
      dispatcher_host_->Send(new QuotaMsg_DidFail(
          request_id_, static_cast<WebStorageQuotaError>(status)));
    } else {
      dispatcher_host_->Send(new QuotaMsg_DidGrantStorageQuota(
          request_id_, granted_quota));
    }
    dispatcher_host_->outstanding_requests_.Remove(request_id_);
  }

  QuotaDispatcherHost* dispatcher_host_;
  quota::QuotaManager* quota_manager_;
  int request_id_;
  base::ScopedCallbackFactory<RequestDispatcher> callback_factory_;
};

QuotaDispatcherHost::QuotaDispatcherHost(QuotaManager* quota_manager)
    : quota_manager_(quota_manager) {
}

QuotaDispatcherHost::~QuotaDispatcherHost() {
}

bool QuotaDispatcherHost::OnMessageReceived(
    const IPC::Message& message, bool* message_was_ok) {
  *message_was_ok = true;
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(QuotaDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(QuotaHostMsg_QueryStorageUsageAndQuota,
                        OnQueryStorageUsageAndQuota)
    IPC_MESSAGE_HANDLER(QuotaHostMsg_RequestStorageQuota,
                        OnRequestStorageQuota)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void QuotaDispatcherHost::OnQueryStorageUsageAndQuota(
    int request_id,
    const GURL& origin,
    WebKit::WebStorageQuotaType type) {
  RequestDispatcher* dispatcher = new RequestDispatcher(
      this, quota_manager_, request_id);
  dispatcher->QueryStorageUsageAndQuota(origin, static_cast<StorageType>(type));
}

void QuotaDispatcherHost::OnRequestStorageQuota(
    int request_id,
    const GURL& origin,
    WebKit::WebStorageQuotaType type,
    int64 requested_size) {
  RequestDispatcher* dispatcher = new RequestDispatcher(
      this, quota_manager_, request_id);
  dispatcher->RequestStorageQuota(origin, static_cast<StorageType>(type),
                                  requested_size);
}

COMPILE_ASSERT(int(WebKit::WebStorageQuotaTypeTemporary) == \
               int(quota::kStorageTypeTemporary), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebStorageQuotaTypePersistent) == \
               int(quota::kStorageTypePersistent), mismatching_enums);

COMPILE_ASSERT(int(WebKit::WebStorageQuotaErrorNotSupported) == \
               int(quota::kQuotaErrorNotSupported), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebStorageQuotaErrorAbort) == \
               int(quota::kQuotaErrorAbort), mismatching_enums);
