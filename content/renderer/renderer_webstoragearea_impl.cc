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

// In November 2011 stats were recorded about performance of each of the
// DOMStorage operations. Results of median, 99% quantile, and 99.9% quantile
// are provided in milliseconds. The ratio of number of calls for each operation
// relative to the number of calls to getItem is also provided.
//
// Operation    Freq     50%     99%     99.9%
// -------------------------------------------
// getItem      1.00     0.6     2.0     27.9
// setItem      .029     0.7     13.6    114.9
// removeItem   .003     0.9     11.8    90.7
// length       .017     0.6     2.0     12.0
// key          .591     0.6     2.0     29.9
// clear        1e-6     1.0     32.4    605.2

unsigned RendererWebStorageAreaImpl::length() {
  unsigned length;
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_Length(storage_area_id_, &length));
  return length;
}

WebString RendererWebStorageAreaImpl::key(unsigned index) {
  NullableString16 key;
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_Key(storage_area_id_, index, &key));
  return key;
}

WebString RendererWebStorageAreaImpl::getItem(const WebString& key) {
  NullableString16 value;
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_GetItem(storage_area_id_, key, &value));
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
  RenderThreadImpl::current()->Send(new DOMStorageHostMsg_SetItem(
      storage_area_id_, key, value, url, &result, &old_value));
  old_value_webkit = old_value;
}

void RendererWebStorageAreaImpl::removeItem(
    const WebString& key, const WebURL& url, WebString& old_value_webkit) {
  NullableString16 old_value;
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_RemoveItem(storage_area_id_, key, url, &old_value));
  old_value_webkit = old_value;
}

void RendererWebStorageAreaImpl::clear(
    const WebURL& url, bool& cleared_something) {
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_Clear(storage_area_id_, url, &cleared_something));
}
