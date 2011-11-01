// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webstoragearea_impl.h"

#include "base/metrics/histogram.h"
#include "base/time.h"
#include "content/common/dom_storage_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"

using WebKit::WebStorageNamespace;
using WebKit::WebString;
using WebKit::WebURL;

RendererWebStorageAreaImpl::RendererWebStorageAreaImpl(
    int64 namespace_id, const WebString& origin) {
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_StorageAreaId(namespace_id, origin,
                                          &storage_area_id_));
}

RendererWebStorageAreaImpl::~RendererWebStorageAreaImpl() {
}

unsigned RendererWebStorageAreaImpl::length() {
  unsigned length;
  base::Time start = base::Time::Now();
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_Length(storage_area_id_, &length));
  UMA_HISTOGRAM_TIMES("DOMStorage.length", base::Time::Now() - start);
  return length;
}

WebString RendererWebStorageAreaImpl::key(unsigned index) {
  NullableString16 key;
  base::Time start = base::Time::Now();
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_Key(storage_area_id_, index, &key));
  UMA_HISTOGRAM_TIMES("DOMStorage.key", base::Time::Now() - start);
  return key;
}

WebString RendererWebStorageAreaImpl::getItem(const WebString& key) {
  NullableString16 value;
  base::Time start = base::Time::Now();
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_GetItem(storage_area_id_, key, &value));
  UMA_HISTOGRAM_TIMES("DOMStorage.getItem", base::Time::Now() - start);
  return value;
}

void RendererWebStorageAreaImpl::setItem(
    const WebString& key, const WebString& value, const WebURL& url,
    WebStorageArea::Result& result, WebString& old_value_webkit) {
  const size_t kMaxKeyValueLength = WebStorageNamespace::m_localStorageQuota;
  if (key.length() + value.length() > kMaxKeyValueLength) {
    result = ResultBlockedByQuota;
    return;
  }
  NullableString16 old_value;
  base::Time start = base::Time::Now();
  RenderThreadImpl::current()->Send(new DOMStorageHostMsg_SetItem(
      storage_area_id_, key, value, url, &result, &old_value));
  UMA_HISTOGRAM_TIMES("DOMStorage.setItem", base::Time::Now() - start);
  old_value_webkit = old_value;
}

void RendererWebStorageAreaImpl::removeItem(
    const WebString& key, const WebURL& url, WebString& old_value_webkit) {
  NullableString16 old_value;
  base::Time start = base::Time::Now();
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_RemoveItem(storage_area_id_, key, url, &old_value));
  UMA_HISTOGRAM_TIMES("DOMStorage.removeItem", base::Time::Now() - start);
  old_value_webkit = old_value;
}

void RendererWebStorageAreaImpl::clear(
    const WebURL& url, bool& cleared_something) {
  base::Time start = base::Time::Now();
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_Clear(storage_area_id_, url, &cleared_something));
  UMA_HISTOGRAM_TIMES("DOMStorage.clear", base::Time::Now() - start);
}
