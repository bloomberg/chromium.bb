// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/quota_dispatcher.h"

#include "base/basictypes.h"
#include "content/common/child_thread.h"
#include "content/common/quota_messages.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageQuotaCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageQuotaType.h"

using quota::QuotaStatusCode;
using quota::StorageType;

using WebKit::WebStorageQuotaCallbacks;
using WebKit::WebStorageQuotaError;
using WebKit::WebStorageQuotaType;

namespace content {
namespace {

// QuotaDispatcher::Callback implementation for WebStorageQuotaCallbacks.
class WebStorageQuotaDispatcherCallback : public QuotaDispatcher::Callback {
 public:
  WebStorageQuotaDispatcherCallback(WebKit::WebStorageQuotaCallbacks* callback)
      : callbacks_(callback) {
    DCHECK(callbacks_);
  }
  virtual ~WebStorageQuotaDispatcherCallback() {}
  virtual void DidQueryStorageUsageAndQuota(int64 usage, int64 quota) OVERRIDE {
    callbacks_->didQueryStorageUsageAndQuota(usage, quota);
  }
  virtual void DidGrantStorageQuota(int64 granted_quota) OVERRIDE {
    callbacks_->didGrantStorageQuota(granted_quota);
  }
  virtual void DidFail(quota::QuotaStatusCode error) OVERRIDE {
    callbacks_->didFail(static_cast<WebStorageQuotaError>(error));
  }

 private:
  // Not owned (self-destructed).
  WebKit::WebStorageQuotaCallbacks* callbacks_;
};

}  // namespace

QuotaDispatcher::QuotaDispatcher() {
}

QuotaDispatcher::~QuotaDispatcher() {
  IDMap<Callback, IDMapOwnPointer>::iterator iter(&pending_quota_callbacks_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->DidFail(quota::kQuotaErrorAbort);
    iter.Advance();
  }
}

bool QuotaDispatcher::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(QuotaDispatcher, msg)
    IPC_MESSAGE_HANDLER(QuotaMsg_DidGrantStorageQuota,
                        DidGrantStorageQuota)
    IPC_MESSAGE_HANDLER(QuotaMsg_DidQueryStorageUsageAndQuota,
                        DidQueryStorageUsageAndQuota);
    IPC_MESSAGE_HANDLER(QuotaMsg_DidFail, DidFail);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void QuotaDispatcher::QueryStorageUsageAndQuota(
    const GURL& origin_url,
    StorageType type,
    Callback* callback) {
  DCHECK(callback);
  int request_id = pending_quota_callbacks_.Add(callback);
  ChildThread::current()->Send(new QuotaHostMsg_QueryStorageUsageAndQuota(
      request_id, origin_url, type));
}

void QuotaDispatcher::RequestStorageQuota(
    int render_view_id,
    const GURL& origin_url,
    StorageType type,
    int64 requested_size,
    Callback* callback) {
  DCHECK(callback);
  int request_id = pending_quota_callbacks_.Add(callback);
  ChildThread::current()->Send(new QuotaHostMsg_RequestStorageQuota(
      render_view_id, request_id, origin_url, type, requested_size));
}

// static
QuotaDispatcher::Callback*
QuotaDispatcher::CreateWebStorageQuotaCallbacksWrapper(
    WebKit::WebStorageQuotaCallbacks* callbacks) {
  return new WebStorageQuotaDispatcherCallback(callbacks);
}

void QuotaDispatcher::DidGrantStorageQuota(
    int request_id,
    int64 granted_quota) {
  Callback* callback = pending_quota_callbacks_.Lookup(request_id);
  DCHECK(callback);
  callback->DidGrantStorageQuota(granted_quota);
  pending_quota_callbacks_.Remove(request_id);
}

void QuotaDispatcher::DidQueryStorageUsageAndQuota(
    int request_id,
    int64 current_usage,
    int64 current_quota) {
  Callback* callback = pending_quota_callbacks_.Lookup(request_id);
  DCHECK(callback);
  callback->DidQueryStorageUsageAndQuota(current_usage, current_quota);
  pending_quota_callbacks_.Remove(request_id);
}

void QuotaDispatcher::DidFail(
    int request_id,
    QuotaStatusCode error) {
  Callback* callback = pending_quota_callbacks_.Lookup(request_id);
  DCHECK(callback);
  callback->DidFail(error);
  pending_quota_callbacks_.Remove(request_id);
}

COMPILE_ASSERT(int(WebKit::WebStorageQuotaTypeTemporary) == \
               int(quota::kStorageTypeTemporary), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebStorageQuotaTypePersistent) == \
               int(quota::kStorageTypePersistent), mismatching_enums);

COMPILE_ASSERT(int(WebKit::WebStorageQuotaErrorNotSupported) == \
               int(quota::kQuotaErrorNotSupported), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebStorageQuotaErrorAbort) == \
               int(quota::kQuotaErrorAbort), mismatching_enums);

}  // namespace content
