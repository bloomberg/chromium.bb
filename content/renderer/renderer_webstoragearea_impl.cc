// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webstoragearea_impl.h"

// TODO(jam): temporary include until webkit merge
#include "chrome/renderer/content_settings_observer.h"

#include "content/common/dom_storage_messages.h"
#include "content/renderer/render_thread.h"
#include "content/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"

using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebView;

RendererWebStorageAreaImpl::RendererWebStorageAreaImpl(
    int64 namespace_id, const WebString& origin, DOMStorageType storage_type)
    : storage_type_(storage_type) {
  RenderThread::current()->Send(
      new DOMStorageHostMsg_StorageAreaId(namespace_id, origin,
                                          &storage_area_id_));
}

RendererWebStorageAreaImpl::~RendererWebStorageAreaImpl() {
}

unsigned RendererWebStorageAreaImpl::length() {
  unsigned length;
  RenderThread::current()->Send(
      new DOMStorageHostMsg_Length(storage_area_id_, &length));
  return length;
}

WebString RendererWebStorageAreaImpl::key(unsigned index) {
  NullableString16 key;
  RenderThread::current()->Send(
      new DOMStorageHostMsg_Key(storage_area_id_, index, &key));
  return key;
}

WebString RendererWebStorageAreaImpl::getItem(const WebString& key) {
  NullableString16 value;
  RenderThread::current()->Send(
      new DOMStorageHostMsg_GetItem(storage_area_id_, key, &value));
  return value;
}

void RendererWebStorageAreaImpl::setItem(
    const WebString& key, const WebString& value, const WebURL& url,
    WebStorageArea::Result& result, WebString& old_value_webkit,
    WebFrame* web_frame) {
  int32 render_view_id = MSG_ROUTING_CONTROL;
  if (web_frame) {
    RenderView* render_view = RenderView::FromWebView(web_frame->view());
    if (render_view) {
      render_view_id = render_view->routing_id();

      // TODO(jam): remove after merge
      ContentSettingsObserver* content_setting =
          ContentSettingsObserver::Get(render_view);
      if (!content_setting->AllowStorage(
              web_frame, storage_type_ == DOM_STORAGE_LOCAL)) {
        result = WebStorageArea::ResultBlockedByQuota;
        old_value_webkit = NullableString16();
        return;
      }
    }
  }
  DCHECK(render_view_id != MSG_ROUTING_CONTROL);

  NullableString16 old_value;
  RenderThread::current()->Send(new DOMStorageHostMsg_SetItem(
      render_view_id, storage_area_id_, key, value, url, &result, &old_value));
  old_value_webkit = old_value;
}

void RendererWebStorageAreaImpl::removeItem(
    const WebString& key, const WebURL& url, WebString& old_value_webkit) {
  NullableString16 old_value;
  RenderThread::current()->Send(
      new DOMStorageHostMsg_RemoveItem(storage_area_id_, key, url, &old_value));
  old_value_webkit = old_value;
}

void RendererWebStorageAreaImpl::clear(
    const WebURL& url, bool& cleared_something) {
  RenderThread::current()->Send(
      new DOMStorageHostMsg_Clear(storage_area_id_, url, &cleared_something));
}
