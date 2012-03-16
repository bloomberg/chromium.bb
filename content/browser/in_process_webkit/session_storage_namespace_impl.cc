// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/session_storage_namespace_impl.h"

#ifdef ENABLE_NEW_DOM_STORAGE_BACKEND
// This class is replaced by a new implementation in
// content/browser/dom_storage/session_storage_namespace_impl_new.h
#else

#include "base/bind.h"
#include "base/logging.h"
#include "content/browser/in_process_webkit/dom_storage_context_impl.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    DOMStorageContextImpl* context)
    : dom_storage_context_(context),
      id_(dom_storage_context_->AllocateSessionStorageNamespaceId()) {
}

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    DOMStorageContextImpl* context, int64 id)
    : dom_storage_context_(context),
      id_(id) {
  DCHECK(dom_storage_context_);
}

SessionStorageNamespaceImpl::~SessionStorageNamespaceImpl() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED) &&
      BrowserThread::PostTask(
          BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
          base::Bind(&DOMStorageContextImpl::DeleteSessionStorageNamespace,
                     dom_storage_context_, id_))) {
    return;
  }

  dom_storage_context_->DeleteSessionStorageNamespace(id_);
}

SessionStorageNamespaceImpl* SessionStorageNamespaceImpl::Clone() {
  return new SessionStorageNamespaceImpl(
      dom_storage_context_, dom_storage_context_->CloneSessionStorage(id_));
}

#endif  // ENABLE_NEW_DOM_STORAGE_BACKEND

