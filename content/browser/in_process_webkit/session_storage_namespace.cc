// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/session_storage_namespace.h"

#include "base/logging.h"
#include "content/browser/in_process_webkit/dom_storage_context.h"
#include "content/browser/in_process_webkit/webkit_context.h"

SessionStorageNamespace::SessionStorageNamespace(WebKitContext* webkit_context)
    : webkit_context_(webkit_context),
      id_(webkit_context_->dom_storage_context()
          ->AllocateSessionStorageNamespaceId()) {
}

SessionStorageNamespace::SessionStorageNamespace(WebKitContext* webkit_context,
                                                 int64 id)
    : webkit_context_(webkit_context),
      id_(id) {
  DCHECK(webkit_context_);
}

SessionStorageNamespace::~SessionStorageNamespace() {
  webkit_context_->DeleteSessionStorageNamespace(id_);
}

SessionStorageNamespace* SessionStorageNamespace::Clone() {
  return new SessionStorageNamespace(
      webkit_context_,
      webkit_context_->dom_storage_context()->CloneSessionStorage(id_));
}
