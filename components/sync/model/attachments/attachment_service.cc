// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/attachments/attachment_service.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/sync/engine/attachments/fake_attachment_downloader.h"
#include "components/sync/engine/attachments/fake_attachment_uploader.h"
#include "components/sync/model/attachments/attachment_store.h"
#include "components/sync/model_impl/attachments/attachment_service_impl.h"

namespace syncer {

// static
std::unique_ptr<AttachmentService> AttachmentService::Create(
    std::unique_ptr<AttachmentStoreForSync> attachment_store,
    std::unique_ptr<AttachmentUploader> attachment_uploader,
    std::unique_ptr<AttachmentDownloader> attachment_downloader,
    Delegate* delegate,
    const base::TimeDelta& initial_backoff_delay,
    const base::TimeDelta& max_backoff_delay) {
  return base::MakeUnique<AttachmentServiceImpl>(
      std::move(attachment_store), std::move(attachment_uploader),
      std::move(attachment_downloader), delegate, initial_backoff_delay,
      max_backoff_delay);
}

// static
std::unique_ptr<AttachmentService> AttachmentService::CreateForTest() {
  std::unique_ptr<AttachmentStore> attachment_store =
      AttachmentStore::CreateInMemoryStore();
  return base::MakeUnique<AttachmentServiceImpl>(
      attachment_store->CreateAttachmentStoreForSync(),
      base::MakeUnique<FakeAttachmentUploader>(),
      base::MakeUnique<FakeAttachmentDownloader>(), nullptr, base::TimeDelta(),
      base::TimeDelta());
}

AttachmentService::~AttachmentService() {}

}  // namespace syncer
