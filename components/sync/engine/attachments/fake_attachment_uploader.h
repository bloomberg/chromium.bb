// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ATTACHMENTS_FAKE_ATTACHMENT_UPLOADER_H_
#define COMPONENTS_SYNC_ENGINE_ATTACHMENTS_FAKE_ATTACHMENT_UPLOADER_H_

#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "components/sync/engine/attachments/attachment_uploader.h"

namespace syncer {

// A fake implementation of AttachmentUploader.
class FakeAttachmentUploader : public AttachmentUploader,
                               public base::NonThreadSafe {
 public:
  FakeAttachmentUploader();
  ~FakeAttachmentUploader() override;

  // AttachmentUploader implementation.
  void UploadAttachment(const Attachment& attachment,
                        const UploadCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAttachmentUploader);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ATTACHMENTS_FAKE_ATTACHMENT_UPLOADER_H_
