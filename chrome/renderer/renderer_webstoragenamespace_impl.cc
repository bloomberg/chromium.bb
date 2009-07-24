// Copyright (c) 2009 The Chromium Authors. All rights reserved.  Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/renderer/renderer_webstoragenamespace_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/renderer_webstoragearea_impl.h"

RendererWebStorageNamespaceImpl::RendererWebStorageNamespaceImpl(
    bool is_local_storage)
    : is_local_storage_(is_local_storage),
      namespace_id_(kUninitializedNamespaceId) {
}

RendererWebStorageNamespaceImpl::RendererWebStorageNamespaceImpl(
    bool is_local_storage, int64 namespace_id)
    : is_local_storage_(is_local_storage),
      namespace_id_(namespace_id) {
  DCHECK(namespace_id_ != kUninitializedNamespaceId);
}

RendererWebStorageNamespaceImpl::~RendererWebStorageNamespaceImpl() {
  if (namespace_id_ != kUninitializedNamespaceId)
    RenderThread::current()->Send(
        new ViewHostMsg_DOMStorageDerefNamespaceId(namespace_id_));
}

WebKit::WebStorageArea* RendererWebStorageNamespaceImpl::createStorageArea(
    const WebKit::WebString& origin) {
  // This could be done async in the background (started when this class is
  // first instantiated) rather than lazily on first use, but it's unclear
  // whether it's worth the complexity.
  if (namespace_id_ == kUninitializedNamespaceId) {
    RenderThread::current()->Send(
        new ViewHostMsg_DOMStorageNamespaceId(is_local_storage_,
                                              &namespace_id_));
    DCHECK(namespace_id_ != kUninitializedNamespaceId);
  }
  // Ideally, we'd keep a hash map of origin to these objects.  Unfortunately
  // this doesn't seem practical because there's no good way to ref-count these
  // objects, and it'd be unclear who owned them.  So, instead, we'll pay a
  // price for an allocaiton and IPC for each.
  return new RendererWebStorageAreaImpl(namespace_id_, origin);
}

WebKit::WebStorageNamespace* RendererWebStorageNamespaceImpl::copy() {
  // If we haven't been used yet, we might as well start out fresh (and lazy).
  if (namespace_id_ == kUninitializedNamespaceId)
    return new RendererWebStorageNamespaceImpl(is_local_storage_);

  // This cannot easily be differed because we need a snapshot in time.
  int64 new_namespace_id;
  RenderThread::current()->Send(
      new ViewHostMsg_DOMStorageCloneNamespaceId(namespace_id_,
                                                 &new_namespace_id));
  return new RendererWebStorageNamespaceImpl(is_local_storage_,
                                             new_namespace_id);
}

void RendererWebStorageNamespaceImpl::close() {
  // This is called only on LocalStorage namespaces when WebKit thinks its
  // shutting down.  This has no impact on Chromium.
}
