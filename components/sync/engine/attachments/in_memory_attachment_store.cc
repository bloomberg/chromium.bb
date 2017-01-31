// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/attachments/in_memory_attachment_store.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"

namespace syncer {

namespace {

void AppendMetadata(AttachmentMetadataList* list,
                    const Attachment& attachment) {
  list->push_back(
      AttachmentMetadata(attachment.GetId(), attachment.GetData()->size()));
}

}  // namespace

InMemoryAttachmentStore::InMemoryAttachmentStore(
    const scoped_refptr<base::SequencedTaskRunner>& callback_task_runner)
    : AttachmentStoreBackend(callback_task_runner) {
  // Object is created on one thread but used on another.
  DetachFromThread();
}

InMemoryAttachmentStore::~InMemoryAttachmentStore() {}

void InMemoryAttachmentStore::Init(
    const AttachmentStore::InitCallback& callback) {
  DCHECK(CalledOnValidThread());
  PostCallback(base::Bind(callback, AttachmentStore::SUCCESS));
}

void InMemoryAttachmentStore::Read(
    AttachmentStore::Component component,
    const AttachmentIdList& ids,
    const AttachmentStore::ReadCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentStore::Result result_code = AttachmentStore::SUCCESS;
  std::unique_ptr<AttachmentMap> result_map(new AttachmentMap);
  std::unique_ptr<AttachmentIdList> unavailable_attachments(
      new AttachmentIdList);

  for (const auto& id : ids) {
    AttachmentEntryMap::iterator iter = attachments_.find(id);
    if (iter != attachments_.end() &&
        iter->second.components.count(component) > 0) {
      const Attachment& attachment = iter->second.attachment;
      result_map->insert(std::make_pair(id, attachment));
    } else {
      unavailable_attachments->push_back(id);
    }
  }
  if (!unavailable_attachments->empty()) {
    result_code = AttachmentStore::UNSPECIFIED_ERROR;
  }
  PostCallback(base::Bind(callback, result_code, base::Passed(&result_map),
                          base::Passed(&unavailable_attachments)));
}

void InMemoryAttachmentStore::Write(
    AttachmentStore::Component component,
    const AttachmentList& attachments,
    const AttachmentStore::WriteCallback& callback) {
  DCHECK(CalledOnValidThread());
  for (const auto& attachment : attachments) {
    attachments_.insert(std::make_pair(attachment.GetId(),
                                       AttachmentEntry(attachment, component)));
  }
  PostCallback(base::Bind(callback, AttachmentStore::SUCCESS));
}

void InMemoryAttachmentStore::SetReference(AttachmentStore::Component component,
                                           const AttachmentIdList& ids) {
  DCHECK(CalledOnValidThread());
  for (const auto& id : ids) {
    AttachmentEntryMap::iterator attachments_iter = attachments_.find(id);
    if (attachments_iter != attachments_.end()) {
      attachments_iter->second.components.insert(component);
    }
  }
}

void InMemoryAttachmentStore::DropReference(
    AttachmentStore::Component component,
    const AttachmentIdList& ids,
    const AttachmentStore::DropCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentStore::Result result = AttachmentStore::SUCCESS;
  for (const auto& id : ids) {
    AttachmentEntryMap::iterator attachments_iter = attachments_.find(id);
    if (attachments_iter == attachments_.end()) {
      continue;
    }
    attachments_iter->second.components.erase(component);
    if (attachments_iter->second.components.empty()) {
      attachments_.erase(attachments_iter);
    }
  }
  PostCallback(base::Bind(callback, result));
}

void InMemoryAttachmentStore::ReadMetadataById(
    AttachmentStore::Component component,
    const AttachmentIdList& ids,
    const AttachmentStore::ReadMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentStore::Result result_code = AttachmentStore::SUCCESS;
  std::unique_ptr<AttachmentMetadataList> metadata_list(
      new AttachmentMetadataList());

  for (const auto& id : ids) {
    AttachmentEntryMap::iterator iter = attachments_.find(id);
    if (iter == attachments_.end()) {
      result_code = AttachmentStore::UNSPECIFIED_ERROR;
      continue;
    }
    DCHECK_GT(iter->second.components.size(), 0u);
    if (iter->second.components.count(component) == 0) {
      result_code = AttachmentStore::UNSPECIFIED_ERROR;
      continue;
    }
    AppendMetadata(metadata_list.get(), iter->second.attachment);
  }
  PostCallback(base::Bind(callback, result_code, base::Passed(&metadata_list)));
}

void InMemoryAttachmentStore::ReadMetadata(
    AttachmentStore::Component component,
    const AttachmentStore::ReadMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  AttachmentStore::Result result_code = AttachmentStore::SUCCESS;
  std::unique_ptr<AttachmentMetadataList> metadata_list(
      new AttachmentMetadataList());

  for (AttachmentEntryMap::const_iterator iter = attachments_.begin();
       iter != attachments_.end(); ++iter) {
    DCHECK_GT(iter->second.components.size(), 0u);
    if (iter->second.components.count(component) > 0) {
      AppendMetadata(metadata_list.get(), iter->second.attachment);
    }
  }
  PostCallback(base::Bind(callback, result_code, base::Passed(&metadata_list)));
}

InMemoryAttachmentStore::AttachmentEntry::AttachmentEntry(
    const Attachment& attachment,
    AttachmentStore::Component initial_reference_component)
    : attachment(attachment) {
  components.insert(initial_reference_component);
}

InMemoryAttachmentStore::AttachmentEntry::AttachmentEntry(
    const AttachmentEntry& other) = default;

InMemoryAttachmentStore::AttachmentEntry::~AttachmentEntry() {}

}  // namespace syncer
