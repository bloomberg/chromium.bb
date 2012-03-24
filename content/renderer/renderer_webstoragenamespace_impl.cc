// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webstoragenamespace_impl.h"

#include "base/logging.h"
#include "content/renderer/renderer_webstoragearea_impl.h"

using WebKit::WebStorageArea;
using WebKit::WebStorageNamespace;
using WebKit::WebString;

RendererWebStorageNamespaceImpl::RendererWebStorageNamespaceImpl(
    DOMStorageType storage_type)
    : namespace_id_(kLocalStorageNamespaceId) {
  DCHECK(storage_type == DOM_STORAGE_LOCAL);
}

RendererWebStorageNamespaceImpl::RendererWebStorageNamespaceImpl(
    DOMStorageType storage_type, int64 namespace_id)
    : namespace_id_(namespace_id) {
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
  // By returning NULL, we're telling WebKit to lazily fetch it the next time
  // session storage is used.  In the WebViewClient::createView, we do the
  // book-keeping necessary to make it a true copy-on-write despite not doing
  // anything here, now.
  return NULL;
}

void RendererWebStorageNamespaceImpl::close() {
  // This is called only on LocalStorage namespaces when WebKit thinks its
  // shutting down.  This has no impact on Chromium.
}
