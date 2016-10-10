// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/attachments/attachment_store_frontend.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "components/sync/engine/attachments/attachment_store_backend.h"
#include "components/sync/model/attachments/attachment.h"

namespace syncer {

namespace {

// NoOp is needed to bind base::Passed(backend) in AttachmentStoreFrontend dtor.
// It doesn't need to do anything.
void NoOp(std::unique_ptr<AttachmentStoreBackend> backend) {}

}  // namespace

AttachmentStoreFrontend::AttachmentStoreFrontend(
    std::unique_ptr<AttachmentStoreBackend> backend,
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner)
    : backend_(std::move(backend)), backend_task_runner_(backend_task_runner) {
  DCHECK(backend_);
  DCHECK(backend_task_runner_.get());
}

AttachmentStoreFrontend::~AttachmentStoreFrontend() {
  DCHECK(backend_);
  // To delete backend post task that doesn't do anything, but binds backend
  // through base::Passed. This way backend will be deleted regardless whether
  // task runs or not.
  backend_task_runner_->PostTask(FROM_HERE,
                                 base::Bind(&NoOp, base::Passed(&backend_)));
}

void AttachmentStoreFrontend::Init(
    const AttachmentStore::InitCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AttachmentStoreBackend::Init,
                            base::Unretained(backend_.get()), callback));
}

void AttachmentStoreFrontend::Read(
    AttachmentStore::Component component,
    const AttachmentIdList& ids,
    const AttachmentStore::ReadCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentStoreBackend::Read,
                 base::Unretained(backend_.get()), component, ids, callback));
}

void AttachmentStoreFrontend::Write(
    AttachmentStore::Component component,
    const AttachmentList& attachments,
    const AttachmentStore::WriteCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AttachmentStoreBackend::Write,
                            base::Unretained(backend_.get()), component,
                            attachments, callback));
}

void AttachmentStoreFrontend::SetReference(AttachmentStore::Component component,
                                           const AttachmentIdList& ids) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AttachmentStoreBackend::SetReference,
                            base::Unretained(backend_.get()), component, ids));
}

void AttachmentStoreFrontend::DropReference(
    AttachmentStore::Component component,
    const AttachmentIdList& ids,
    const AttachmentStore::DropCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentStoreBackend::DropReference,
                 base::Unretained(backend_.get()), component, ids, callback));
}

void AttachmentStoreFrontend::ReadMetadataById(
    AttachmentStore::Component component,
    const AttachmentIdList& ids,
    const AttachmentStore::ReadMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentStoreBackend::ReadMetadataById,
                 base::Unretained(backend_.get()), component, ids, callback));
}

void AttachmentStoreFrontend::ReadMetadata(
    AttachmentStore::Component component,
    const AttachmentStore::ReadMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  backend_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentStoreBackend::ReadMetadata,
                 base::Unretained(backend_.get()), component, callback));
}

}  // namespace syncer
