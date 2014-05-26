// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/render_messages.h"
#include "chrome/renderer/worker_permission_client_proxy.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/WebKit/public/platform/WebPermissionCallbacks.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"

WorkerPermissionClientProxy::WorkerPermissionClientProxy(
    content::RenderFrame* render_frame,
    blink::WebFrame* frame)
    : routing_id_(render_frame->GetRoutingID()),
      is_unique_origin_(false) {
  if (frame->document().securityOrigin().isUnique() ||
      frame->top()->document().securityOrigin().isUnique())
    is_unique_origin_ = true;
  sync_message_filter_ = content::RenderThread::Get()->GetSyncMessageFilter();
  document_origin_url_ = GURL(frame->document().securityOrigin().toString());
  top_frame_origin_url_ = GURL(
      frame->top()->document().securityOrigin().toString());
}

WorkerPermissionClientProxy::~WorkerPermissionClientProxy() {}

bool WorkerPermissionClientProxy::allowDatabase(
    const blink::WebString& name,
    const blink::WebString& display_name,
    unsigned long estimated_size) {
  if (is_unique_origin_)
    return false;

  bool result = false;
  sync_message_filter_->Send(new ChromeViewHostMsg_AllowDatabase(
      routing_id_, document_origin_url_, top_frame_origin_url_,
      name, display_name, &result));
  return result;
}

bool WorkerPermissionClientProxy::requestFileSystemAccessSync() {
  if (is_unique_origin_)
    return false;

  bool result = false;
  sync_message_filter_->Send(new ChromeViewHostMsg_RequestFileSystemAccessSync(
      routing_id_, document_origin_url_, top_frame_origin_url_, &result));
  return result;
}

bool WorkerPermissionClientProxy::allowIndexedDB(
    const blink::WebString& name) {
  if (is_unique_origin_)
    return false;

  bool result = false;
  sync_message_filter_->Send(new ChromeViewHostMsg_AllowIndexedDB(
      routing_id_, document_origin_url_, top_frame_origin_url_, name, &result));
  return result;
}
