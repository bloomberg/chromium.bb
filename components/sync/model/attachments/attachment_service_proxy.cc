// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model/attachments/attachment_service_proxy.h"

#include <memory>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"

namespace syncer {

namespace {

// These ProxyFooCallback functions are used to invoke a callback in a specific
// thread.

// Invokes |callback| with |result| and |attachments| in the |task_runner|
// thread.
void ProxyGetOrDownloadCallback(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const AttachmentService::GetOrDownloadCallback& callback,
    const AttachmentService::GetOrDownloadResult& result,
    std::unique_ptr<AttachmentMap> attachments) {
  task_runner->PostTask(
      FROM_HERE, base::Bind(callback, result, base::Passed(&attachments)));
}

}  // namespace

AttachmentServiceProxy::AttachmentServiceProxy() {}

AttachmentServiceProxy::AttachmentServiceProxy(
    const scoped_refptr<base::SequencedTaskRunner>& wrapped_task_runner,
    const base::WeakPtr<AttachmentService>& wrapped)
    : wrapped_task_runner_(wrapped_task_runner), core_(new Core(wrapped)) {
  DCHECK(wrapped_task_runner_.get());
}

AttachmentServiceProxy::AttachmentServiceProxy(
    const scoped_refptr<base::SequencedTaskRunner>& wrapped_task_runner,
    const scoped_refptr<Core>& core)
    : wrapped_task_runner_(wrapped_task_runner), core_(core) {
  DCHECK(wrapped_task_runner_.get());
  DCHECK(core_.get());
}

AttachmentServiceProxy::AttachmentServiceProxy(
    const AttachmentServiceProxy& other) = default;

AttachmentServiceProxy::~AttachmentServiceProxy() {}

void AttachmentServiceProxy::GetOrDownloadAttachments(
    const AttachmentIdList& attachment_ids,
    const GetOrDownloadCallback& callback) {
  DCHECK(wrapped_task_runner_.get());
  GetOrDownloadCallback proxy_callback =
      base::Bind(&ProxyGetOrDownloadCallback,
                 base::ThreadTaskRunnerHandle::Get(), callback);
  wrapped_task_runner_->PostTask(
      FROM_HERE, base::Bind(&AttachmentService::GetOrDownloadAttachments, core_,
                            attachment_ids, proxy_callback));
}

void AttachmentServiceProxy::UploadAttachments(
    const AttachmentIdList& attachment_ids) {
  DCHECK(wrapped_task_runner_.get());
  wrapped_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&AttachmentService::UploadAttachments, core_, attachment_ids));
}

AttachmentServiceProxy::Core::Core(
    const base::WeakPtr<AttachmentService>& wrapped)
    : wrapped_(wrapped) {}

AttachmentServiceProxy::Core::~Core() {}

void AttachmentServiceProxy::Core::GetOrDownloadAttachments(
    const AttachmentIdList& attachment_ids,
    const GetOrDownloadCallback& callback) {
  if (!wrapped_) {
    return;
  }
  wrapped_->GetOrDownloadAttachments(attachment_ids, callback);
}

void AttachmentServiceProxy::Core::UploadAttachments(
    const AttachmentIdList& attachment_ids) {
  if (!wrapped_) {
    return;
  }
  wrapped_->UploadAttachments(attachment_ids);
}

}  // namespace syncer
