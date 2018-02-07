// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_namespace_impl.h"

#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/dom_storage_session.h"
#include "content/browser/dom_storage/session_storage_context_mojo.h"

namespace content {

// static
scoped_refptr<SessionStorageNamespaceImpl> SessionStorageNamespaceImpl::Create(
    DOMStorageContextWrapper* context) {
  return base::WrapRefCounted(
      new SessionStorageNamespaceImpl(DOMStorageSession::Create(
          context->context(), context->GetMojoSessionStateWeakPtr())));
}

// static
scoped_refptr<SessionStorageNamespaceImpl> SessionStorageNamespaceImpl::Create(
    DOMStorageContextWrapper* context,
    const std::string& namepace_id) {
  return base::WrapRefCounted(
      new SessionStorageNamespaceImpl(DOMStorageSession::Create(
          context->context(), context->GetMojoSessionStateWeakPtr(),
          namepace_id)));
}

// static
scoped_refptr<SessionStorageNamespaceImpl>
SessionStorageNamespaceImpl::CloneFrom(
    DOMStorageContextWrapper* context,
    const std::string& namepace_id_to_clone) {
  return base::WrapRefCounted(
      new SessionStorageNamespaceImpl(DOMStorageSession::CloneFrom(
          context->context(), context->GetMojoSessionStateWeakPtr(),
          namepace_id_to_clone)));
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
  return session_->IsFromContext(context->context());
}

SessionStorageNamespaceImpl::SessionStorageNamespaceImpl(
    scoped_refptr<DOMStorageSession> session)
    : session_(std::move(session)) {}

SessionStorageNamespaceImpl::~SessionStorageNamespaceImpl() {
}

}  // namespace content
