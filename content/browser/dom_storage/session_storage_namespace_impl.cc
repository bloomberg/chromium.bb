// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_namespace_impl.h"

#include <utility>

#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/dom_storage_session.h"
#include "content/browser/dom_storage/session_storage_context_mojo.h"

namespace content {

// static
scoped_refptr<SessionStorageNamespaceImpl> SessionStorageNamespaceImpl::Create(
    scoped_refptr<DOMStorageContextWrapper> context) {
  return base::WrapRefCounted(new SessionStorageNamespaceImpl(
      DOMStorageSession::Create(std::move(context))));
}

// static
scoped_refptr<SessionStorageNamespaceImpl> SessionStorageNamespaceImpl::Create(
    scoped_refptr<DOMStorageContextWrapper> context,
    const std::string& namepace_id) {
  scoped_refptr<SessionStorageNamespaceImpl> existing =
      context->MaybeGetExistingNamespace(namepace_id);
  if (!existing) {
    existing = base::WrapRefCounted(
        new SessionStorageNamespaceImpl(DOMStorageSession::CreateWithNamespace(
            std::move(context), namepace_id)));
  }
  return existing;
}

// static
scoped_refptr<SessionStorageNamespaceImpl>
SessionStorageNamespaceImpl::CloneFrom(
    scoped_refptr<DOMStorageContextWrapper> context,
    std::string namepace_id,
    const std::string& namepace_id_to_clone) {
  return base::WrapRefCounted(
      new SessionStorageNamespaceImpl(DOMStorageSession::CloneFrom(
          std::move(context), std::move(namepace_id), namepace_id_to_clone)));
}

const std::string& SessionStorageNamespaceImpl::id() const {
  return session_->namespace_id();
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
    DOMStorageContextWrapper* context) {
  return session_->IsFromContext(context);
}

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    std::unique_ptr<DOMStorageSession> session)
    : session_(std::move(session)) {
  session_->context()->AddNamespace(session_->namespace_id(), this);
}

SessionStorageNamespaceImpl::~SessionStorageNamespaceImpl() {
  session_->context()->RemoveNamespace(session_->namespace_id());
}

}  // namespace content
