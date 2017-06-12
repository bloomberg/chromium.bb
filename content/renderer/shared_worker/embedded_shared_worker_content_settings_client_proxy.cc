// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/shared_worker/embedded_shared_worker_content_settings_client_proxy.h"

#include "content/child/thread_safe_sender.h"
#include "content/common/worker_messages.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "url/gurl.h"

namespace content {

EmbeddedSharedWorkerContentSettingsClientProxy::
    EmbeddedSharedWorkerContentSettingsClientProxy(
        const GURL& origin_url,
        bool is_unique_origin,
        int routing_id,
        ThreadSafeSender* thread_safe_sender)
    : origin_url_(origin_url),
      is_unique_origin_(is_unique_origin),
      routing_id_(routing_id),
      thread_safe_sender_(thread_safe_sender) {
}

EmbeddedSharedWorkerContentSettingsClientProxy::
    ~EmbeddedSharedWorkerContentSettingsClientProxy() {
}

bool EmbeddedSharedWorkerContentSettingsClientProxy::
    RequestFileSystemAccessSync() {
  if (is_unique_origin_)
    return false;
  bool result = false;
  thread_safe_sender_->Send(
      new WorkerProcessHostMsg_RequestFileSystemAccessSync(
          routing_id_, origin_url_, &result));
  return result;
}

bool EmbeddedSharedWorkerContentSettingsClientProxy::AllowIndexedDB(
    const blink::WebString& name,
    const blink::WebSecurityOrigin&) {
  if (is_unique_origin_)
    return false;
  bool result = false;
  thread_safe_sender_->Send(new WorkerProcessHostMsg_AllowIndexedDB(
      routing_id_, origin_url_, name.Utf16(), &result));
  return result;
}

}  // namespace content
