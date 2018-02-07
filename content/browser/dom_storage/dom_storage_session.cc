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

// static
scoped_refptr<DOMStorageSession> DOMStorageSession::Create(
    DOMStorageContextImpl* context,
    base::WeakPtr<SessionStorageContextMojo> mojo_context) {
  scoped_refptr<DOMStorageSession> result =
      base::WrapRefCounted(new DOMStorageSession(
          context, std::move(mojo_context), context->AllocateSessionId()));
  context->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DOMStorageContextImpl::CreateSessionNamespace,
                                context, result->namespace_id()));
  if (result->mojo_context_)
    result->mojo_context_->CreateSessionNamespace(result->namespace_id());

  return result;
}

// static
scoped_refptr<DOMStorageSession> DOMStorageSession::Create(
    DOMStorageContextImpl* context,
    base::WeakPtr<SessionStorageContextMojo> mojo_context,
    const std::string& namespace_id) {
  context->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DOMStorageContextImpl::CreateSessionNamespace,
                                context, namespace_id));
  if (mojo_context)
    mojo_context->CreateSessionNamespace(namespace_id);

  return base::WrapRefCounted(
      new DOMStorageSession(context, std::move(mojo_context), namespace_id));
}

// static
scoped_refptr<DOMStorageSession> DOMStorageSession::CloneFrom(
    DOMStorageContextImpl* context,
    base::WeakPtr<SessionStorageContextMojo> mojo_context,
    const std::string& namespace_id_to_clone) {
  std::string clone_id = context->AllocateSessionId();
  context->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DOMStorageContextImpl::CloneSessionNamespace,
                                context, namespace_id_to_clone, clone_id));
  if (mojo_context)
    mojo_context->CloneSessionNamespace(namespace_id_to_clone, clone_id);
  return base::WrapRefCounted(
      new DOMStorageSession(context, std::move(mojo_context), clone_id));
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

scoped_refptr<DOMStorageSession> DOMStorageSession::Clone() {
  return CloneFrom(context_.get(), mojo_context_, namespace_id_);
}

DOMStorageSession::DOMStorageSession(
    DOMStorageContextImpl* context,
    base::WeakPtr<SessionStorageContextMojo> mojo_context,
    const std::string& namespace_id)
    : context_(context),
      mojo_context_(mojo_context),
      namespace_id_(namespace_id),
      should_persist_(false) {}

DOMStorageSession::~DOMStorageSession() {
  context_->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DOMStorageContextImpl::DeleteSessionNamespace,
                                context_, namespace_id_, should_persist_));
  if (mojo_context_)
    mojo_context_->DeleteSessionNamespace(namespace_id_, should_persist_);
}

}  // namespace content
