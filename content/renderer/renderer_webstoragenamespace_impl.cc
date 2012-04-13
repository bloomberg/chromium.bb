// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_webstoragenamespace_impl.h"

#include "base/logging.h"
#include "content/renderer/renderer_webstoragearea_impl.h"

using WebKit::WebStorageArea;
using WebKit::WebStorageNamespace;
using WebKit::WebString;

RendererWebStorageNamespaceImpl::RendererWebStorageNamespaceImpl()
    : namespace_id_(dom_storage::kLocalStorageNamespaceId) {
}

RendererWebStorageNamespaceImpl::RendererWebStorageNamespaceImpl(
    int64 namespace_id)
    : namespace_id_(namespace_id) {
  DCHECK_NE(dom_storage::kInvalidSessionStorageNamespaceId, namespace_id);
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
  // TOOD(michaeln): remove this deprecated method.
}
