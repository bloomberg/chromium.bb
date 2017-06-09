// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/worker_content_settings_client.h"

#include "chrome/common/render_messages.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_sync_message_filter.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "url/origin.h"

WorkerContentSettingsClient::WorkerContentSettingsClient(
    content::RenderFrame* render_frame,
    blink::WebFrame* frame)
    : routing_id_(render_frame->GetRoutingID()), is_unique_origin_(false) {
  if (frame->GetDocument().GetSecurityOrigin().IsUnique() ||
      frame->Top()->GetSecurityOrigin().IsUnique())
    is_unique_origin_ = true;
  sync_message_filter_ = content::RenderThread::Get()->GetSyncMessageFilter();
  document_origin_url_ =
      url::Origin(frame->GetDocument().GetSecurityOrigin()).GetURL();
  top_frame_origin_url_ =
      url::Origin(frame->Top()->GetSecurityOrigin()).GetURL();
}

WorkerContentSettingsClient::~WorkerContentSettingsClient() {}

bool WorkerContentSettingsClient::RequestFileSystemAccessSync() {
  if (is_unique_origin_)
    return false;

  bool result = false;
  sync_message_filter_->Send(new ChromeViewHostMsg_RequestFileSystemAccessSync(
      routing_id_, document_origin_url_, top_frame_origin_url_, &result));
  return result;
}

bool WorkerContentSettingsClient::AllowIndexedDB(
    const blink::WebString& name,
    const blink::WebSecurityOrigin&) {
  if (is_unique_origin_)
    return false;

  bool result = false;
  sync_message_filter_->Send(new ChromeViewHostMsg_AllowIndexedDB(
      routing_id_, document_origin_url_, top_frame_origin_url_, name.Utf16(),
      &result));
  return result;
}
