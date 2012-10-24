// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_namespace_impl.h"

#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "webkit/dom_storage/dom_storage_session.h"

using dom_storage::DomStorageContext;
using dom_storage::DomStorageSession;

namespace content {

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    DOMStorageContextImpl* context)
    : session_(new DomStorageSession(context->context())) {
}

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    DOMStorageContextImpl* context, int64 namepace_id_to_clone)
    : session_(DomStorageSession::CloneFrom(context->context(),
                                            namepace_id_to_clone)) {
}

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    DOMStorageContextImpl* context, const std::string& persistent_id)
    : session_(new DomStorageSession(context->context(), persistent_id)) {
}

int64 SessionStorageNamespaceImpl::id() const {
  return session_->namespace_id();
}

const std::string& SessionStorageNamespaceImpl::persistent_id() const {
  return session_->persistent_namespace_id();
}

void SessionStorageNamespaceImpl::SetShouldPersist(bool should_persist) {
  session_->SetShouldPersist(should_persist);
}

bool SessionStorageNamespaceImpl::should_persist() const {
  return session_->should_persist();
}

SessionStorageNamespaceImpl* SessionStorageNamespaceImpl::Clone() {
  return new SessionStorageNamespaceImpl(session_->Clone());
}

bool SessionStorageNamespaceImpl::IsFromContext(
    DOMStorageContextImpl* context) {
  return session_->IsFromContext(context->context());
}

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    DomStorageSession* clone)
    : session_(clone) {
}

SessionStorageNamespaceImpl::~SessionStorageNamespaceImpl() {
}

}  // namespace content
