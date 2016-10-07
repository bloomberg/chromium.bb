// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_ATTACHMENTS_FAKE_ATTACHMENT_DOWNLOADER_H_
#define COMPONENTS_SYNC_ENGINE_ATTACHMENTS_FAKE_ATTACHMENT_DOWNLOADER_H_

#include "base/macros.h"
#include "base/threading/non_thread_safe.h"
#include "components/sync/engine/attachments/attachment_downloader.h"

namespace syncer {

// FakeAttachmentDownloader is for tests. For every request it posts a success
// callback with empty attachment.
class FakeAttachmentDownloader : public AttachmentDownloader,
                                 public base::NonThreadSafe {
 public:
  FakeAttachmentDownloader();
  ~FakeAttachmentDownloader() override;

  // AttachmentDownloader implementation.
  void DownloadAttachment(const AttachmentId& attachment_id,
                          const DownloadCallback& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeAttachmentDownloader);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_ATTACHMENTS_FAKE_ATTACHMENT_DOWNLOADER_H_
