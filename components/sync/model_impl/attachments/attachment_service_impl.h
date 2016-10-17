// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_IMPL_ATTACHMENTS_ATTACHMENT_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_MODEL_IMPL_ATTACHMENTS_ATTACHMENT_SERVICE_IMPL_H_

#include <deque>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "components/sync/engine/attachments/attachment_downloader.h"
#include "components/sync/engine/attachments/attachment_uploader.h"
#include "components/sync/model/attachments/attachment_service.h"
#include "components/sync/model/attachments/attachment_service_proxy.h"
#include "components/sync/model/attachments/attachment_store.h"
#include "components/sync/model_impl/attachments/task_queue.h"
#include "net/base/network_change_notifier.h"

namespace syncer {

// Implementation of AttachmentService.
class AttachmentServiceImpl
    : public AttachmentService,
      public net::NetworkChangeNotifier::NetworkChangeObserver,
      public base::NonThreadSafe {
 public:
  // |attachment_store| is required. UploadAttachments reads attachment data
  // from it. Downloaded attachments will be written into it.
  //
  // |attachment_uploader| is optional. If null, attachments will never be
  // uploaded to the sync server and |delegate|'s OnAttachmentUploaded will
  // never be invoked.
  //
  // |attachment_downloader| is optional. If null, attachments will never be
  // downloaded. Only attachments in |attachment_store| will be returned from
  // GetOrDownloadAttachments.
  //
  // |delegate| is optional delegate for AttachmentService to notify about
  // asynchronous events (AttachmentUploaded). Pass null if delegate is not
  // provided. AttachmentService doesn't take ownership of delegate, the pointer
  // must be valid throughout AttachmentService lifetime.
  //
  // |initial_backoff_delay| the initial delay between upload attempts.  This
  // class automatically retries failed uploads.  After the first failure, it
  // will wait this amount of time until it tries again.  After each failure,
  // the delay is doubled until the |max_backoff_delay| is reached.  A
  // successful upload clears the delay.
  //
  // |max_backoff_delay| the maxmium delay between upload attempts when backed
  // off.
  AttachmentServiceImpl(
      std::unique_ptr<AttachmentStoreForSync> attachment_store,
      std::unique_ptr<AttachmentUploader> attachment_uploader,
      std::unique_ptr<AttachmentDownloader> attachment_downloader,
      Delegate* delegate,
      const base::TimeDelta& initial_backoff_delay,
      const base::TimeDelta& max_backoff_delay);
  ~AttachmentServiceImpl() override;

  // AttachmentService implementation.
  void GetOrDownloadAttachments(const AttachmentIdList& attachment_ids,
                                const GetOrDownloadCallback& callback) override;
  void UploadAttachments(const AttachmentIdList& attachment_ids) override;

  // NetworkChangeObserver implementation.
  void OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType type) override;

  // Use |timer| in the underlying TaskQueue.
  //
  // Used in tests.  See also MockTimer.
  void SetTimerForTest(std::unique_ptr<base::Timer> timer);

 private:
  class GetOrDownloadState;

  void ReadDone(const scoped_refptr<GetOrDownloadState>& state,
                const AttachmentStore::Result& result,
                std::unique_ptr<AttachmentMap> attachments,
                std::unique_ptr<AttachmentIdList> unavailable_attachment_ids);
  void WriteDone(const scoped_refptr<GetOrDownloadState>& state,
                 const Attachment& attachment,
                 const AttachmentStore::Result& result);
  void UploadDone(const AttachmentUploader::UploadResult& result,
                  const AttachmentId& attachment_id);
  void DownloadDone(const scoped_refptr<GetOrDownloadState>& state,
                    const AttachmentId& attachment_id,
                    const AttachmentDownloader::DownloadResult& result,
                    std::unique_ptr<Attachment> attachment);
  void BeginUpload(const AttachmentId& attachment_id);
  void ReadDoneNowUpload(
      const AttachmentStore::Result& result,
      std::unique_ptr<AttachmentMap> attachments,
      std::unique_ptr<AttachmentIdList> unavailable_attachment_ids);

  std::unique_ptr<AttachmentStoreForSync> attachment_store_;

  // May be null.
  const std::unique_ptr<AttachmentUploader> attachment_uploader_;

  // May be null.
  const std::unique_ptr<AttachmentDownloader> attachment_downloader_;

  // May be null.
  Delegate* delegate_;

  std::unique_ptr<TaskQueue<AttachmentId>> upload_task_queue_;

  // Must be last data member.
  base::WeakPtrFactory<AttachmentServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AttachmentServiceImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_IMPL_ATTACHMENTS_ATTACHMENT_SERVICE_IMPL_H_
