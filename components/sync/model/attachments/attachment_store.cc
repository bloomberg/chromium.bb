// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/attachments/attachment_store.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/engine/attachments/attachment_store_frontend.h"
#include "components/sync/engine/attachments/in_memory_attachment_store.h"
#include "components/sync/engine/attachments/on_disk_attachment_store.h"

namespace syncer {

namespace {

void NoOpDropCallback(const AttachmentStore::Result& result) {}
}

AttachmentStore::AttachmentStore(
    const scoped_refptr<AttachmentStoreFrontend>& frontend,
    Component component)
    : frontend_(frontend), component_(component) {}

AttachmentStore::~AttachmentStore() {}

void AttachmentStore::Read(const AttachmentIdList& ids,
                           const ReadCallback& callback) {
  frontend_->Read(component_, ids, callback);
}

void AttachmentStore::Write(const AttachmentList& attachments,
                            const WriteCallback& callback) {
  frontend_->Write(component_, attachments, callback);
}

void AttachmentStore::Drop(const AttachmentIdList& ids,
                           const DropCallback& callback) {
  frontend_->DropReference(component_, ids, callback);
}

void AttachmentStore::ReadMetadataById(const AttachmentIdList& ids,
                                       const ReadMetadataCallback& callback) {
  frontend_->ReadMetadataById(component_, ids, callback);
}

void AttachmentStore::ReadMetadata(const ReadMetadataCallback& callback) {
  frontend_->ReadMetadata(component_, callback);
}

std::unique_ptr<AttachmentStoreForSync>
AttachmentStore::CreateAttachmentStoreForSync() const {
  std::unique_ptr<AttachmentStoreForSync> attachment_store_for_sync(
      new AttachmentStoreForSync(frontend_, component_, SYNC));
  return attachment_store_for_sync;
}

std::unique_ptr<AttachmentStore> AttachmentStore::CreateInMemoryStore() {
  // Both frontend and backend of attachment store will live on current thread.
  scoped_refptr<base::SingleThreadTaskRunner> runner;
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    runner = base::ThreadTaskRunnerHandle::Get();
  } else {
    // Dummy runner for tests that don't have MessageLoop.
    base::MessageLoop loop;
    // This works because |runner| takes a ref to the proxy.
    runner = base::ThreadTaskRunnerHandle::Get();
  }
  std::unique_ptr<AttachmentStoreBackend> backend(
      new InMemoryAttachmentStore(runner));
  scoped_refptr<AttachmentStoreFrontend> frontend(
      new AttachmentStoreFrontend(std::move(backend), runner));
  std::unique_ptr<AttachmentStore> attachment_store(
      new AttachmentStore(frontend, MODEL_TYPE));
  return attachment_store;
}

std::unique_ptr<AttachmentStore> AttachmentStore::CreateOnDiskStore(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner,
    const InitCallback& callback) {
  std::unique_ptr<OnDiskAttachmentStore> backend(
      new OnDiskAttachmentStore(base::ThreadTaskRunnerHandle::Get(), path));

  scoped_refptr<AttachmentStoreFrontend> frontend =
      new AttachmentStoreFrontend(std::move(backend), backend_task_runner);
  std::unique_ptr<AttachmentStore> attachment_store(
      new AttachmentStore(frontend, MODEL_TYPE));
  frontend->Init(callback);

  return attachment_store;
}

std::unique_ptr<AttachmentStore> AttachmentStore::CreateMockStoreForTest(
    std::unique_ptr<AttachmentStoreBackend> backend) {
  scoped_refptr<base::SingleThreadTaskRunner> runner =
      base::ThreadTaskRunnerHandle::Get();
  scoped_refptr<AttachmentStoreFrontend> attachment_store_frontend(
      new AttachmentStoreFrontend(std::move(backend), runner));
  std::unique_ptr<AttachmentStore> attachment_store(
      new AttachmentStore(attachment_store_frontend, MODEL_TYPE));
  return attachment_store;
}

AttachmentStoreForSync::AttachmentStoreForSync(
    const scoped_refptr<AttachmentStoreFrontend>& frontend,
    Component consumer_component,
    Component sync_component)
    : AttachmentStore(frontend, consumer_component),
      sync_component_(sync_component) {}

AttachmentStoreForSync::~AttachmentStoreForSync() {}

void AttachmentStoreForSync::SetSyncReference(const AttachmentIdList& ids) {
  frontend()->SetReference(sync_component_, ids);
}

void AttachmentStoreForSync::SetModelTypeReference(
    const AttachmentIdList& ids) {
  frontend()->SetReference(component(), ids);
}

void AttachmentStoreForSync::DropSyncReference(const AttachmentIdList& ids) {
  frontend()->DropReference(sync_component_, ids,
                            base::Bind(&NoOpDropCallback));
}

void AttachmentStoreForSync::ReadMetadataForSync(
    const ReadMetadataCallback& callback) {
  frontend()->ReadMetadata(sync_component_, callback);
}

}  // namespace syncer
