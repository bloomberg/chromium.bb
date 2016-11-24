// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_SERVICE_H_
#define COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_SERVICE_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/sync/model/attachments/attachment.h"

namespace syncer {

class AttachmentDownloader;
class AttachmentStoreForSync;
class AttachmentUploader;
class SyncData;

// AttachmentService is responsible for managing a model type's attachments.
//
// Outside of sync code, AttachmentService shouldn't be used directly. Instead
// use the functionality provided by SyncData and SyncChangeProcessor.
//
// Destroying this object does not necessarily cancel outstanding async
// operations. If you need cancel like semantics, use WeakPtr in the callbacks.
class AttachmentService {
 public:
  // The result of a GetOrDownloadAttachments operation.
  enum GetOrDownloadResult {
    GET_SUCCESS,            // No error, all attachments returned.
    GET_UNSPECIFIED_ERROR,  // An unspecified error occurred.
  };

  typedef base::Callback<void(const GetOrDownloadResult&,
                              std::unique_ptr<AttachmentMap> attachments)>
      GetOrDownloadCallback;

  // An interface that embedder code implements to be notified about different
  // events that originate from AttachmentService.
  // This interface will be called from the same thread AttachmentService was
  // created and called.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Attachment is uploaded to server and attachment_id is updated with server
    // url.
    virtual void OnAttachmentUploaded(const AttachmentId& attachment_id) = 0;
  };

  // Create a concrete AttachmentService.
  static std::unique_ptr<AttachmentService> Create(
      std::unique_ptr<AttachmentStoreForSync> attachment_store,
      std::unique_ptr<AttachmentUploader> attachment_uploader,
      std::unique_ptr<AttachmentDownloader> attachment_downloader,
      Delegate* delegate,
      const base::TimeDelta& initial_backoff_delay,
      const base::TimeDelta& max_backoff_delay);

  // Create an AttachmentService suitable for use in tests.
  static std::unique_ptr<AttachmentService> CreateForTest();

  virtual ~AttachmentService();

  // See SyncData::GetOrDownloadAttachments.
  virtual void GetOrDownloadAttachments(
      const AttachmentIdList& attachment_ids,
      const GetOrDownloadCallback& callback) = 0;

  // Schedules the attachments identified by |attachment_ids| to be uploaded to
  // the server.
  //
  // Assumes the attachments are already in the attachment store.
  //
  // A request to upload attachments is persistent in that uploads will be
  // automatically retried if transient errors occur.
  //
  // A request to upload attachments does not persist across restarts of Chrome.
  //
  // Invokes OnAttachmentUploaded on the Delegate (if provided).
  virtual void UploadAttachments(const AttachmentIdList& attachment_ids) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_ATTACHMENTS_ATTACHMENT_SERVICE_H_
