// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ATTACHMENTS_ATTACHMENT_STORE_FRONTEND_H_
#define COMPONENTS_SYNC_ENGINE_ATTACHMENTS_ATTACHMENT_STORE_FRONTEND_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "components/sync/model/attachments/attachment_store.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace syncer {

class AttachmentStoreBackend;

// AttachmentStoreFrontend is helper to post AttachmentStore calls to backend on
// different thread. Backend is expected to know on which thread to post
// callbacks with results.
// AttachmentStoreFrontend takes ownership of backend. Backend is deleted on
// backend thread.
// AttachmentStoreFrontend is not thread safe, it should only be accessed from
// model thread.
// Method signatures of AttachmentStoreFrontend match exactly methods of
// AttachmentStoreBackend.
class AttachmentStoreFrontend
    : public base::RefCounted<AttachmentStoreFrontend>,
      public base::NonThreadSafe {
 public:
  AttachmentStoreFrontend(
      std::unique_ptr<AttachmentStoreBackend> backend,
      const scoped_refptr<base::SequencedTaskRunner>& backend_task_runner);

  void Init(const AttachmentStore::InitCallback& callback);

  void Read(AttachmentStore::Component component,
            const AttachmentIdList& ids,
            const AttachmentStore::ReadCallback& callback);

  void Write(AttachmentStore::Component component,
             const AttachmentList& attachments,
             const AttachmentStore::WriteCallback& callback);
  void SetReference(AttachmentStore::Component component,
                    const AttachmentIdList& ids);
  void DropReference(AttachmentStore::Component component,
                     const AttachmentIdList& ids,
                     const AttachmentStore::DropCallback& callback);

  void ReadMetadataById(AttachmentStore::Component component,
                        const AttachmentIdList& ids,
                        const AttachmentStore::ReadMetadataCallback& callback);

  void ReadMetadata(AttachmentStore::Component component,
                    const AttachmentStore::ReadMetadataCallback& callback);

 private:
  friend class base::RefCounted<AttachmentStoreFrontend>;
  virtual ~AttachmentStoreFrontend();

  std::unique_ptr<AttachmentStoreBackend> backend_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AttachmentStoreFrontend);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ATTACHMENTS_ATTACHMENT_STORE_FRONTEND_H_
