// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/quota_dispatcher.h"

#include "content/common/child_thread.h"
#include "content/common/quota_messages.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageQuotaCallbacks.h"

using WebKit::WebStorageQuotaCallbacks;
using WebKit::WebStorageQuotaError;
using WebKit::WebStorageQuotaType;

QuotaDispatcher::QuotaDispatcher() {
}

QuotaDispatcher::~QuotaDispatcher() {
  IDMap<WebStorageQuotaCallbacks>::iterator iter(&pending_quota_callbacks_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->didFail(WebKit::WebStorageQuotaErrorAbort);
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
    WebStorageQuotaType type,
    WebStorageQuotaCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_quota_callbacks_.Add(callbacks);
  ChildThread::current()->Send(new QuotaHostMsg_QueryStorageUsageAndQuota(
      request_id, origin_url, type));
}

void QuotaDispatcher::RequestStorageQuota(
    int render_view_id,
    const GURL& origin_url,
    WebStorageQuotaType type,
    unsigned long long requested_size,
    WebStorageQuotaCallbacks* callbacks) {
  DCHECK(callbacks);
  int request_id = pending_quota_callbacks_.Add(callbacks);
  ChildThread::current()->Send(new QuotaHostMsg_RequestStorageQuota(
      render_view_id, request_id, origin_url, type, requested_size));
}

void QuotaDispatcher::DidGrantStorageQuota(
    int request_id,
    int64 granted_quota) {
  WebStorageQuotaCallbacks* callbacks = pending_quota_callbacks_.Lookup(
      request_id);
  DCHECK(callbacks);
  callbacks->didGrantStorageQuota(granted_quota);
  pending_quota_callbacks_.Remove(request_id);
}

void QuotaDispatcher::DidQueryStorageUsageAndQuota(
    int request_id,
    int64 current_usage,
    int64 current_quota) {
  WebStorageQuotaCallbacks* callbacks = pending_quota_callbacks_.Lookup(
      request_id);
  DCHECK(callbacks);
  callbacks->didQueryStorageUsageAndQuota(current_usage, current_quota);
  pending_quota_callbacks_.Remove(request_id);
}

void QuotaDispatcher::DidFail(
    int request_id,
    WebStorageQuotaError error) {
  WebStorageQuotaCallbacks* callbacks = pending_quota_callbacks_.Lookup(
      request_id);
  DCHECK(callbacks);
  callbacks->didFail(error);
  pending_quota_callbacks_.Remove(request_id);
}
