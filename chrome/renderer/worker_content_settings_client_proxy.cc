// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/worker_content_settings_client_proxy.h"

#include "chrome/common/render_messages.h"
#include "components/content_settings/content/common/content_settings_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"

WorkerContentSettingsClientProxy::WorkerContentSettingsClientProxy(
    content::RenderFrame* render_frame,
    blink::WebFrame* frame)
    : routing_id_(render_frame->GetRoutingID()),
      is_unique_origin_(false) {
  if (frame->document().securityOrigin().isUnique() ||
      frame->top()->securityOrigin().isUnique())
    is_unique_origin_ = true;
  sync_message_filter_ = content::RenderThread::Get()->GetSyncMessageFilter();
  document_origin_url_ =
      blink::WebStringToGURL(frame->document().securityOrigin().toString());
  top_frame_origin_url_ =
      blink::WebStringToGURL(frame->top()->securityOrigin().toString());
}

WorkerContentSettingsClientProxy::~WorkerContentSettingsClientProxy() {}

bool WorkerContentSettingsClientProxy::allowDatabase(
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

bool WorkerContentSettingsClientProxy::requestFileSystemAccessSync() {
  if (is_unique_origin_)
    return false;

  bool result = false;
  sync_message_filter_->Send(new ChromeViewHostMsg_RequestFileSystemAccessSync(
      routing_id_, document_origin_url_, top_frame_origin_url_, &result));
  return result;
}

bool WorkerContentSettingsClientProxy::allowIndexedDB(
    const blink::WebString& name) {
  if (is_unique_origin_)
    return false;

  bool result = false;
  sync_message_filter_->Send(new ChromeViewHostMsg_AllowIndexedDB(
      routing_id_, document_origin_url_, top_frame_origin_url_, name, &result));
  return result;
}
