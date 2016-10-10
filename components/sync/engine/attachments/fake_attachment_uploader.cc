// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/attachments/fake_attachment_uploader.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/model/attachments/attachment.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

FakeAttachmentUploader::FakeAttachmentUploader() {
  DCHECK(CalledOnValidThread());
}

FakeAttachmentUploader::~FakeAttachmentUploader() {
  DCHECK(CalledOnValidThread());
}

void FakeAttachmentUploader::UploadAttachment(const Attachment& attachment,
                                              const UploadCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!attachment.GetId().GetProto().unique_id().empty());

  UploadResult result = UPLOAD_SUCCESS;
  AttachmentId id = attachment.GetId();
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, result, id));
}

}  // namespace syncer
