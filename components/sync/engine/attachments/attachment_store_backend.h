// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ATTACHMENTS_ATTACHMENT_STORE_BACKEND_H_
#define COMPONENTS_SYNC_ENGINE_ATTACHMENTS_ATTACHMENT_STORE_BACKEND_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/sync/model/attachments/attachment.h"
#include "components/sync/model/attachments/attachment_id.h"
#include "components/sync/model/attachments/attachment_store.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace syncer {

// Interface for AttachmentStore backends.
//
// AttachmentStoreBackend provides interface for different backends (on-disk,
// in-memory). Factory methods in AttachmentStore create corresponding backend
// and pass reference to AttachmentStore.
// All functions in AttachmentStoreBackend mirror corresponding functions in
// AttachmentStore.
// All callbacks and result codes are used directly from AttachmentStore.
// AttachmentStoreFrontend only passes callbacks and results without modifying
// them, there is no need to declare separate set.
class AttachmentStoreBackend {
 public:
  explicit AttachmentStoreBackend(
      const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner);
  virtual ~AttachmentStoreBackend();
  virtual void Init(const AttachmentStore::InitCallback& callback) = 0;
  virtual void Read(AttachmentStore::Component component,
                    const AttachmentIdList& ids,
                    const AttachmentStore::ReadCallback& callback) = 0;
  virtual void Write(AttachmentStore::Component component,
                     const AttachmentList& attachments,
                     const AttachmentStore::WriteCallback& callback) = 0;
  virtual void SetReference(AttachmentStore::Component component,
                            const AttachmentIdList& ids) = 0;
  virtual void DropReference(AttachmentStore::Component component,
                             const AttachmentIdList& ids,
                             const AttachmentStore::DropCallback& callback) = 0;
  virtual void ReadMetadataById(
      AttachmentStore::Component component,
      const AttachmentIdList& ids,
      const AttachmentStore::ReadMetadataCallback& callback) = 0;
  virtual void ReadMetadata(
      AttachmentStore::Component component,
      const AttachmentStore::ReadMetadataCallback& callback) = 0;

 protected:
  // Helper function to post callback on callback_task_runner.
  void PostCallback(const base::Closure& callback);

 private:
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AttachmentStoreBackend);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ATTACHMENTS_ATTACHMENT_STORE_BACKEND_H_
