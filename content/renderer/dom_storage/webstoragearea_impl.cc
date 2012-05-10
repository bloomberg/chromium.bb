// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/webstoragearea_impl.h"

#include "base/lazy_instance.h"
#include "base/metrics/histogram.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "content/common/dom_storage_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "webkit/dom_storage/dom_storage_types.h"

using WebKit::WebString;
using WebKit::WebURL;

typedef IDMap<WebStorageAreaImpl> AreaImplMap;

static base::LazyInstance<AreaImplMap>::Leaky
    g_all_areas_map = LAZY_INSTANCE_INITIALIZER;

// static
WebStorageAreaImpl* WebStorageAreaImpl::FromConnectionId(
    int id) {
  return g_all_areas_map.Pointer()->Lookup(id);
}

WebStorageAreaImpl::WebStorageAreaImpl(
    int64 namespace_id, const GURL& origin)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          connection_id_(g_all_areas_map.Pointer()->Add(this))) {
  DCHECK(connection_id_);
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_OpenStorageArea(
          connection_id_, namespace_id, origin));
}

WebStorageAreaImpl::~WebStorageAreaImpl() {
  g_all_areas_map.Pointer()->Remove(connection_id_);
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_CloseStorageArea(connection_id_));
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

unsigned WebStorageAreaImpl::length() {
  unsigned length;
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_Length(connection_id_, &length));
  return length;
}

WebString WebStorageAreaImpl::key(unsigned index) {
  NullableString16 key;
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_Key(connection_id_, index, &key));
  return key;
}

WebString WebStorageAreaImpl::getItem(const WebString& key) {
  NullableString16 value;
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_GetItem(connection_id_, key, &value));
  return value;
}

void WebStorageAreaImpl::setItem(
    const WebString& key, const WebString& value, const WebURL& url,
    WebStorageArea::Result& result, WebString& old_value_webkit) {
  if (key.length() + value.length() > dom_storage::kPerAreaQuota) {
    result = ResultBlockedByQuota;
    return;
  }
  NullableString16 old_value;
  RenderThreadImpl::current()->Send(new DOMStorageHostMsg_SetItem(
      connection_id_, key, value, url, &result, &old_value));
  old_value_webkit = old_value;
}

void WebStorageAreaImpl::removeItem(
    const WebString& key, const WebURL& url, WebString& old_value_webkit) {
  NullableString16 old_value;
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_RemoveItem(connection_id_, key, url, &old_value));
  old_value_webkit = old_value;
}

void WebStorageAreaImpl::clear(
    const WebURL& url, bool& cleared_something) {
  RenderThreadImpl::current()->Send(
      new DOMStorageHostMsg_Clear(connection_id_, url, &cleared_something));
}
