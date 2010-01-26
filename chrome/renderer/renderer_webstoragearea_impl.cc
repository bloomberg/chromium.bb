// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/renderer_webstoragearea_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"

using WebKit::WebString;
using WebKit::WebURL;

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
    bool& quota_exception, WebString& old_value_webkit) {
  NullableString16 old_value;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageSetItem(storage_area_id_, key, value, url,
                                        &quota_exception, &old_value));
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
