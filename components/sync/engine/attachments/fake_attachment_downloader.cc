// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/attachments/fake_attachment_downloader.h"

#include <memory>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/engine/attachments/attachment_util.h"

namespace syncer {

FakeAttachmentDownloader::FakeAttachmentDownloader() {}

FakeAttachmentDownloader::~FakeAttachmentDownloader() {
  DCHECK(CalledOnValidThread());
}

void FakeAttachmentDownloader::DownloadAttachment(
    const AttachmentId& attachment_id,
    const DownloadCallback& callback) {
  DCHECK(CalledOnValidThread());
  // This is happy fake downloader, it always successfully downloads empty
  // attachment.
  scoped_refptr<base::RefCountedMemory> data(new base::RefCountedBytes());
  std::unique_ptr<Attachment> attachment = base::MakeUnique<Attachment>(
      Attachment::CreateFromParts(attachment_id, data));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(callback, DOWNLOAD_SUCCESS, base::Passed(&attachment)));
}

}  // namespace syncer
