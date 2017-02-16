// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ATTACHMENTS_IN_MEMORY_ATTACHMENT_STORE_H_
#define COMPONENTS_SYNC_ENGINE_ATTACHMENTS_IN_MEMORY_ATTACHMENT_STORE_H_

#include <map>
#include <set>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "components/sync/engine/attachments/attachment_store_backend.h"
#include "components/sync/model/attachments/attachment.h"
#include "components/sync/model/attachments/attachment_id.h"
#include "components/sync/model/attachments/attachment_store.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace syncer {

// An in-memory implementation of AttachmentStore used for testing.
// InMemoryAttachmentStore is not threadsafe, it lives on backend thread and
// posts callbacks with results on |callback_task_runner|.
class InMemoryAttachmentStore : public AttachmentStoreBackend,
                                public base::NonThreadSafe {
 public:
  InMemoryAttachmentStore(
      const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner);
  ~InMemoryAttachmentStore() override;

  // AttachmentStoreBackend implementation.
  void Init(const AttachmentStore::InitCallback& callback) override;
  void Read(AttachmentStore::Component component,
            const AttachmentIdList& ids,
            const AttachmentStore::ReadCallback& callback) override;
  void Write(AttachmentStore::Component component,
             const AttachmentList& attachments,
             const AttachmentStore::WriteCallback& callback) override;
  void SetReference(AttachmentStore::Component component,
                    const AttachmentIdList& ids) override;
  void DropReference(AttachmentStore::Component component,
                     const AttachmentIdList& ids,
                     const AttachmentStore::DropCallback& callback) override;
  void ReadMetadataById(
      AttachmentStore::Component component,
      const AttachmentIdList& ids,
      const AttachmentStore::ReadMetadataCallback& callback) override;
  void ReadMetadata(
      AttachmentStore::Component component,
      const AttachmentStore::ReadMetadataCallback& callback) override;

 private:
  struct AttachmentEntry {
    AttachmentEntry(const Attachment& attachment,
                    AttachmentStore::Component initial_reference_component);
    AttachmentEntry(const AttachmentEntry& other);
    ~AttachmentEntry();

    Attachment attachment;
    std::set<AttachmentStore::Component> components;
  };

  using AttachmentEntryMap = std::map<AttachmentId, AttachmentEntry>;
  AttachmentEntryMap attachments_;

  DISALLOW_COPY_AND_ASSIGN(InMemoryAttachmentStore);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ATTACHMENTS_IN_MEMORY_ATTACHMENT_STORE_H_
