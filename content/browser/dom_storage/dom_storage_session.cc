// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/dom_storage_session.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/session_storage_context_mojo.h"
#include "content/common/dom_storage/dom_storage_namespace_ids.h"
#include "content/public/common/content_features.h"

namespace content {

// Constructed on thread that constructs DOMStorageSession. Used & destroyed on
// the mojo task runner that the |mojo_context_| runs on.
class DOMStorageSession::SequenceHelper {
 public:
  SequenceHelper(SessionStorageContextMojo* context_wrapper)
      : context_wrapper_(context_wrapper) {}
  ~SequenceHelper() = default;

  void CreateSessionNamespace(const std::string& namespace_id) {
    context_wrapper_->CreateSessionNamespace(namespace_id);
  }

  void CloneSessionNamespace(const std::string& namespace_id_to_clone,
                             const std::string& clone_namespace_id) {
    context_wrapper_->CloneSessionNamespace(namespace_id_to_clone,
                                            clone_namespace_id);
  }

  void DeleteSessionNamespace(const std::string& namespace_id,
                              bool should_persist) {
    context_wrapper_->DeleteSessionNamespace(namespace_id, should_persist);
  }

 private:
  SessionStorageContextMojo* context_wrapper_;
};

// static
std::unique_ptr<DOMStorageSession> DOMStorageSession::Create(
    scoped_refptr<DOMStorageContextWrapper> context) {
  return DOMStorageSession::CreateWithNamespace(
      std::move(context), AllocateSessionStorageNamespaceId());
}

// static
std::unique_ptr<DOMStorageSession> DOMStorageSession::CreateWithNamespace(
    scoped_refptr<DOMStorageContextWrapper> context,
    const std::string& namespace_id) {
  if (context->mojo_session_state()) {
    DCHECK(base::FeatureList::IsEnabled(features::kMojoSessionStorage));
    std::unique_ptr<DOMStorageSession> result = base::WrapUnique(
        new DOMStorageSession(std::move(context), namespace_id));
    result->mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SequenceHelper::CreateSessionNamespace,
                       base::Unretained(result->sequence_helper_.get()),
                       result->namespace_id_));
    return result;
  }
  scoped_refptr<DOMStorageContextImpl> context_impl = context->context();
  context_impl->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&DOMStorageContextImpl::CreateSessionNamespace,
                                context_impl, namespace_id));
  return std::unique_ptr<DOMStorageSession>(new DOMStorageSession(
      std::move(context), std::move(context_impl), namespace_id));
}

// static
std::unique_ptr<DOMStorageSession> DOMStorageSession::CloneFrom(
    scoped_refptr<DOMStorageContextWrapper> context,
    std::string namepace_id,
    const std::string& namespace_id_to_clone) {
  if (context->mojo_session_state()) {
    DCHECK(base::FeatureList::IsEnabled(features::kMojoSessionStorage));
    std::unique_ptr<DOMStorageSession> result = base::WrapUnique(
        new DOMStorageSession(std::move(context), std::move(namepace_id)));

    result->mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&SequenceHelper::CloneSessionNamespace,
                       base::Unretained(result->sequence_helper_.get()),
                       namespace_id_to_clone, result->namespace_id_));
    return result;
  }
  scoped_refptr<DOMStorageContextImpl> context_impl = context->context();
  context_impl->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DOMStorageContextImpl::CloneSessionNamespace,
                     context_impl, namespace_id_to_clone, namepace_id));
  return std::unique_ptr<DOMStorageSession>(new DOMStorageSession(
      std::move(context), std::move(context_impl), std::move(namepace_id)));
}

void DOMStorageSession::SetShouldPersist(bool should_persist) {
  should_persist_ = should_persist;
}

bool DOMStorageSession::should_persist() const {
  return should_persist_;
}

bool DOMStorageSession::IsFromContext(DOMStorageContextWrapper* context) {
  return context_wrapper_.get() == context;
}

std::unique_ptr<DOMStorageSession> DOMStorageSession::Clone() {
  return CloneFrom(context_wrapper_, AllocateSessionStorageNamespaceId(),
                   namespace_id_);
}

DOMStorageSession::DOMStorageSession(
    scoped_refptr<DOMStorageContextWrapper> context_wrapper,
    scoped_refptr<DOMStorageContextImpl> context,
    std::string namespace_id)
    : context_(std::move(context)),
      context_wrapper_(std::move(context_wrapper)),
      namespace_id_(std::move(namespace_id)),
      should_persist_(false) {
  DCHECK(!base::FeatureList::IsEnabled(features::kMojoSessionStorage));
}

DOMStorageSession::DOMStorageSession(
    scoped_refptr<DOMStorageContextWrapper> context,
    std::string namespace_id)
    : context_wrapper_(std::move(context)),
      mojo_task_runner_(context_wrapper_->mojo_task_runner()),
      namespace_id_(std::move(namespace_id)),
      should_persist_(false),
      sequence_helper_(std::make_unique<SequenceHelper>(
          context_wrapper_->mojo_session_state())) {
  DCHECK(base::FeatureList::IsEnabled(features::kMojoSessionStorage));
}

DOMStorageSession::~DOMStorageSession() {
  DCHECK(!!context_ || !!sequence_helper_);
  if (context_) {
    context_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&DOMStorageContextImpl::DeleteSessionNamespace, context_,
                       namespace_id_, should_persist_));
  }
  if (sequence_helper_) {
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SequenceHelper::DeleteSessionNamespace,
                                  base::Unretained(sequence_helper_.get()),
                                  namespace_id_, should_persist_));
    mojo_task_runner_->DeleteSoon(FROM_HERE, std::move(sequence_helper_));
    context_wrapper_->AddRef();
    if (!mojo_task_runner_->ReleaseSoon(FROM_HERE, context_wrapper_.get()))
      context_wrapper_->Release();
  }
}

}  // namespace content
