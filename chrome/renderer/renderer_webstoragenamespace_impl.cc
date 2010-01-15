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
    : storage_type_(storage_type) {
  RenderThread::current()->Send(
        new ViewHostMsg_DOMStorageNamespaceId(storage_type_,
                                              &namespace_id_));
}

RendererWebStorageNamespaceImpl::RendererWebStorageNamespaceImpl(
    DOMStorageType storage_type, int64 namespace_id)
    : storage_type_(storage_type),
      namespace_id_(namespace_id) {
}

RendererWebStorageNamespaceImpl::~RendererWebStorageNamespaceImpl() {
}

WebStorageArea* RendererWebStorageNamespaceImpl::createStorageArea(
    const WebString& origin) {
  // Ideally, we'd keep a hash map of origin to these objects.  Unfortunately
  // this doesn't seem practical because there's no good way to ref-count these
  // objects, and it'd be unclear who owned them.  So, instead, we'll pay a
  // price for an allocaiton and IPC for each.
  return new RendererWebStorageAreaImpl(namespace_id_, origin);
}

WebStorageNamespace* RendererWebStorageNamespaceImpl::copy() {
  // This cannot easily be deferred because we need a snapshot in time.
  int64 new_namespace_id;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageCloneNamespaceId(namespace_id_,
                                                 &new_namespace_id));
  return new RendererWebStorageNamespaceImpl(storage_type_,
                                             new_namespace_id);
}

void RendererWebStorageNamespaceImpl::close() {
  // This is called only on LocalStorage namespaces when WebKit thinks its
  // shutting down.  This has no impact on Chromium.
}
