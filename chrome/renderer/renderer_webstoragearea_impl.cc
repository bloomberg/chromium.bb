// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/renderer_webstoragearea_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebView;

RendererWebStorageAreaImpl::RendererWebStorageAreaImpl(
    int64 namespace_id, const WebString& origin) {
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageStorageAreaId(namespace_id, origin,
                                              &storage_area_id_));
}

RendererWebStorageAreaImpl::~RendererWebStorageAreaImpl() {
}

unsigned RendererWebStorageAreaImpl::length() {
  unsigned length;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageLength(storage_area_id_, &length));
  return length;
}

WebString RendererWebStorageAreaImpl::key(unsigned index) {
  NullableString16 key;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageKey(storage_area_id_, index, &key));
  return key;
}

WebString RendererWebStorageAreaImpl::getItem(const WebString& key) {
  NullableString16 value;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageGetItem(storage_area_id_, key, &value));
  return value;
}

void RendererWebStorageAreaImpl::setItem(
    const WebString& key, const WebString& value, const WebURL& url,
    WebStorageArea::Result& result, WebString& old_value_webkit,
    WebFrame* web_frame) {
  int32 routing_id = MSG_ROUTING_CONTROL;
  if (web_frame) {
    RenderView* render_view = RenderView::FromWebView(web_frame->view());
    if (render_view)
      routing_id = render_view->routing_id();
  }
  DCHECK(routing_id != MSG_ROUTING_CONTROL);

  NullableString16 old_value;
  IPC::SyncMessage* message =
      new ViewHostMsg_DOMStorageSetItem(routing_id, storage_area_id_, key,
                                        value, url, &result, &old_value);
  // NOTE: This may pump events (see RenderThread::Send).
  RenderThread::current()->Send(message);
  old_value_webkit = old_value;
}

void RendererWebStorageAreaImpl::removeItem(
    const WebString& key, const WebURL& url, WebString& old_value_webkit) {
  NullableString16 old_value;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageRemoveItem(storage_area_id_, key,
                                           url, &old_value));
  old_value_webkit = old_value;
}

void RendererWebStorageAreaImpl::clear(
    const WebURL& url, bool& cleared_something) {
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageClear(storage_area_id_, url,
                                      &cleared_something));
}
