// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/renderer_webstoragenamespace_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/renderer_webstoragearea_impl.h"

using WebKit::WebStorageArea;
using WebKit::WebStorageNamespace;
using WebKit::WebString;

RendererWebStorageNamespaceImpl::RendererWebStorageNamespaceImpl(
    DOMStorageType storage_type)
    : storage_type_(storage_type),
      namespace_id_(kLocalStorageNamespaceId) {
  DCHECK(storage_type == DOM_STORAGE_LOCAL);
}

RendererWebStorageNamespaceImpl::RendererWebStorageNamespaceImpl(
    DOMStorageType storage_type, int64 namespace_id)
    : storage_type_(storage_type),
      namespace_id_(namespace_id) {
  DCHECK(storage_type == DOM_STORAGE_SESSION);
}

RendererWebStorageNamespaceImpl::~RendererWebStorageNamespaceImpl() {
}

WebStorageArea* RendererWebStorageNamespaceImpl::createStorageArea(
    const WebString& origin) {
  // Ideally, we'd keep a hash map of origin to these objects.  Unfortunately
  // this doesn't seem practical because there's no good way to ref-count these
  // objects, and it'd be unclear who owned them.  So, instead, we'll pay the
  // price in terms of wasted memory.
  return new RendererWebStorageAreaImpl(namespace_id_, origin);
}

WebStorageNamespace* RendererWebStorageNamespaceImpl::copy() {
  NOTREACHED();  // We shouldn't ever reach this code in Chromium.
  return NULL;
}

void RendererWebStorageNamespaceImpl::close() {
  // This is called only on LocalStorage namespaces when WebKit thinks its
  // shutting down.  This has no impact on Chromium.
}
