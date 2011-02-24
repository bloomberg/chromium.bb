// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/session_storage_namespace.h"

#include "chrome/browser/profiles/profile.h"
#include "content/browser/in_process_webkit/dom_storage_context.h"
#include "content/browser/in_process_webkit/webkit_context.h"

SessionStorageNamespace::SessionStorageNamespace(Profile* profile)
    : webkit_context_(profile->GetWebKitContext()),
      id_(webkit_context_->dom_storage_context()
          ->AllocateSessionStorageNamespaceId()) {
}

SessionStorageNamespace::SessionStorageNamespace(WebKitContext* webkit_context,
                                                 int64 id)
    : webkit_context_(webkit_context),
      id_(id) {
}

SessionStorageNamespace::~SessionStorageNamespace() {
  webkit_context_->DeleteSessionStorageNamespace(id_);
}

SessionStorageNamespace* SessionStorageNamespace::Clone() {
  return new SessionStorageNamespace(
      webkit_context_,
      webkit_context_->dom_storage_context()->CloneSessionStorage(id_));
}
