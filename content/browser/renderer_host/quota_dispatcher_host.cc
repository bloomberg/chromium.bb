// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/quota_dispatcher_host.h"

#include "content/common/quota_messages.h"
#include "googleurl/src/gurl.h"

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
  // TODO(kinuko): not implemented yet.
  Send(new QuotaMsg_DidFail(
      request_id,
      WebKit::WebStorageQuotaErrorNotSupported));
}

void QuotaDispatcherHost::OnRequestStorageQuota(
    int request_id,
    const GURL& origin,
    WebKit::WebStorageQuotaType type,
    int64 requested_size) {
  // TODO(kinuko): not implemented yet.
  Send(new QuotaMsg_DidFail(
      request_id,
      WebKit::WebStorageQuotaErrorNotSupported));
}
