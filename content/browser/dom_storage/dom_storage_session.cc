// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_session.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/session_storage_context_mojo.h"

namespace content {

DOMStorageSession::DOMStorageSession(
    DOMStorageContextImpl* context,
    base::WeakPtr<SessionStorageContextMojo> mojo_context)
    : context_(context),
      mojo_context_(mojo_context),
      namespace_id_(context->AllocateSessionId()),
      persistent_namespace_id_(context->AllocatePersistentSessionId()),
      should_persist_(false) {
  context->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DOMStorageContextImpl::CreateSessionNamespace, context_,
                     namespace_id_, persistent_namespace_id_));
  if (mojo_context_)
    mojo_context_->CreateSessionNamespace(namespace_id_,
                                          persistent_namespace_id_);
}

DOMStorageSession::DOMStorageSession(
    DOMStorageContextImpl* context,
    base::WeakPtr<SessionStorageContextMojo> mojo_context,
    const std::string& persistent_namespace_id)
    : context_(context),
      mojo_context_(mojo_context),
      namespace_id_(context->AllocateSessionId()),
      persistent_namespace_id_(persistent_namespace_id),
      should_persist_(false) {
  context->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DOMStorageContextImpl::CreateSessionNamespace, context_,
                     namespace_id_, persistent_namespace_id_));
  if (mojo_context_)
    mojo_context_->CreateSessionNamespace(namespace_id_,
                                          persistent_namespace_id_);
}

void DOMStorageSession::SetShouldPersist(bool should_persist) {
  should_persist_ = should_persist;
}

bool DOMStorageSession::should_persist() const {
  return should_persist_;
}

bool DOMStorageSession::IsFromContext(DOMStorageContextImpl* context) {
  return context_.get() == context;
}

DOMStorageSession* DOMStorageSession::Clone() {
  return CloneFrom(context_.get(), mojo_context_, namespace_id_);
}

// static
DOMStorageSession* DOMStorageSession::CloneFrom(
    DOMStorageContextImpl* context,
    base::WeakPtr<SessionStorageContextMojo> mojo_context,
    int64_t namespace_id_to_clone) {
  int64_t clone_id = context->AllocateSessionId();
  std::string persistent_clone_id = context->AllocatePersistentSessionId();
  context->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DOMStorageContextImpl::CloneSessionNamespace, context,
                     namespace_id_to_clone, clone_id, persistent_clone_id));
  if (mojo_context)
    mojo_context->CloneSessionNamespace(namespace_id_to_clone, clone_id,
                                        persistent_clone_id);
  return new DOMStorageSession(context, std::move(mojo_context), clone_id,
                               persistent_clone_id);
}

DOMStorageSession::DOMStorageSession(
    DOMStorageContextImpl* context,
    base::WeakPtr<SessionStorageContextMojo> mojo_context,
    int64_t namespace_id,
    const std::string& persistent_namespace_id)
    : context_(context),
      mojo_context_(mojo_context),
      namespace_id_(namespace_id),
      persistent_namespace_id_(persistent_namespace_id),
      should_persist_(false) {
  // This ctor is intended for use by the Clone() method.
}

DOMStorageSession::~DOMStorageSession() {
  context_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DOMStorageContextImpl::DeleteSessionNamespace,
                                context_, namespace_id_, should_persist_));
  if (mojo_context_)
    mojo_context_->DeleteSessionNamespace(namespace_id_, should_persist_);
}

}  // namespace content
