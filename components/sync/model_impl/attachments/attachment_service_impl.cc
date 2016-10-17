// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/model_impl/attachments/attachment_service_impl.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/sync/engine/attachments/fake_attachment_downloader.h"
#include "components/sync/engine/attachments/fake_attachment_uploader.h"
#include "components/sync/model/attachments/attachment.h"

namespace syncer {

// GetOrDownloadAttachments starts multiple parallel DownloadAttachment calls.
// GetOrDownloadState tracks completion of these calls and posts callback for
// consumer once all attachments are either retrieved or reported unavailable.
class AttachmentServiceImpl::GetOrDownloadState
    : public base::RefCounted<GetOrDownloadState>,
      public base::NonThreadSafe {
 public:
  // GetOrDownloadState gets parameter from values passed to
  // AttachmentService::GetOrDownloadAttachments.
  // |attachment_ids| is a list of attachmens to retrieve.
  // |callback| will be posted on current thread when all attachments retrieved
  // or confirmed unavailable.
  GetOrDownloadState(const AttachmentIdList& attachment_ids,
                     const GetOrDownloadCallback& callback);

  // Attachment was just retrieved. Add it to retrieved attachments.
  void AddAttachment(const Attachment& attachment);

  // Both reading from local store and downloading attachment failed.
  // Add it to unavailable set.
  void AddUnavailableAttachmentId(const AttachmentId& attachment_id);

 private:
  friend class base::RefCounted<GetOrDownloadState>;
  virtual ~GetOrDownloadState();

  // If all attachment requests completed then post callback to consumer with
  // results.
  void PostResultIfAllRequestsCompleted();

  GetOrDownloadCallback callback_;

  // Requests for these attachments are still in progress.
  AttachmentIdSet in_progress_attachments_;

  AttachmentIdSet unavailable_attachments_;
  std::unique_ptr<AttachmentMap> retrieved_attachments_;

  DISALLOW_COPY_AND_ASSIGN(GetOrDownloadState);
};

AttachmentServiceImpl::GetOrDownloadState::GetOrDownloadState(
    const AttachmentIdList& attachment_ids,
    const GetOrDownloadCallback& callback)
    : callback_(callback), retrieved_attachments_(new AttachmentMap()) {
  std::copy(
      attachment_ids.begin(), attachment_ids.end(),
      std::inserter(in_progress_attachments_, in_progress_attachments_.end()));
  PostResultIfAllRequestsCompleted();
}

AttachmentServiceImpl::GetOrDownloadState::~GetOrDownloadState() {
  DCHECK(CalledOnValidThread());
}

void AttachmentServiceImpl::GetOrDownloadState::AddAttachment(
    const Attachment& attachment) {
  DCHECK(CalledOnValidThread());
  DCHECK(retrieved_attachments_->find(attachment.GetId()) ==
         retrieved_attachments_->end());
  retrieved_attachments_->insert(
      std::make_pair(attachment.GetId(), attachment));
  DCHECK(in_progress_attachments_.find(attachment.GetId()) !=
         in_progress_attachments_.end());
  in_progress_attachments_.erase(attachment.GetId());
  PostResultIfAllRequestsCompleted();
}

void AttachmentServiceImpl::GetOrDownloadState::AddUnavailableAttachmentId(
    const AttachmentId& attachment_id) {
  DCHECK(CalledOnValidThread());
  DCHECK(unavailable_attachments_.find(attachment_id) ==
         unavailable_attachments_.end());
  unavailable_attachments_.insert(attachment_id);
  DCHECK(in_progress_attachments_.find(attachment_id) !=
         in_progress_attachments_.end());
  in_progress_attachments_.erase(attachment_id);
  PostResultIfAllRequestsCompleted();
}

void AttachmentServiceImpl::GetOrDownloadState::
    PostResultIfAllRequestsCompleted() {
  if (in_progress_attachments_.empty()) {
    // All requests completed. Let's notify consumer.
    GetOrDownloadResult result =
        unavailable_attachments_.empty() ? GET_SUCCESS : GET_UNSPECIFIED_ERROR;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(callback_, result, base::Passed(&retrieved_attachments_)));
  }
}

AttachmentServiceImpl::AttachmentServiceImpl(
    std::unique_ptr<AttachmentStoreForSync> attachment_store,
    std::unique_ptr<AttachmentUploader> attachment_uploader,
    std::unique_ptr<AttachmentDownloader> attachment_downloader,
    Delegate* delegate,
    const base::TimeDelta& initial_backoff_delay,
    const base::TimeDelta& max_backoff_delay)
    : attachment_store_(std::move(attachment_store)),
      attachment_uploader_(std::move(attachment_uploader)),
      attachment_downloader_(std::move(attachment_downloader)),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  DCHECK(CalledOnValidThread());
  DCHECK(attachment_store_.get());

  // TODO(maniscalco): Observe network connectivity change events.  When the
  // network becomes disconnected, consider suspending queue dispatch.  When
  // connectivity is restored, consider clearing any dispatch backoff (bug
  // 411981).
  upload_task_queue_ = base::MakeUnique<TaskQueue<AttachmentId>>(
      base::Bind(&AttachmentServiceImpl::BeginUpload,
                 weak_ptr_factory_.GetWeakPtr()),
      initial_backoff_delay, max_backoff_delay);

  net::NetworkChangeNotifier::AddNetworkChangeObserver(this);
}

AttachmentServiceImpl::~AttachmentServiceImpl() {
  DCHECK(CalledOnValidThread());
  net::NetworkChangeNotifier::RemoveNetworkChangeObserver(this);
}

void AttachmentServiceImpl::GetOrDownloadAttachments(
    const AttachmentIdList& attachment_ids,
    const GetOrDownloadCallback& callback) {
  DCHECK(CalledOnValidThread());
  scoped_refptr<GetOrDownloadState> state(
      new GetOrDownloadState(attachment_ids, callback));
  // SetModelTypeReference() makes attachments visible for model type.
  // Needed when attachment doesn't have model type reference, but still
  // available in local store.
  attachment_store_->SetModelTypeReference(attachment_ids);
  attachment_store_->Read(attachment_ids,
                          base::Bind(&AttachmentServiceImpl::ReadDone,
                                     weak_ptr_factory_.GetWeakPtr(), state));
}

void AttachmentServiceImpl::ReadDone(
    const scoped_refptr<GetOrDownloadState>& state,
    const AttachmentStore::Result& result,
    std::unique_ptr<AttachmentMap> attachments,
    std::unique_ptr<AttachmentIdList> unavailable_attachment_ids) {
  // Add read attachments to result.
  for (AttachmentMap::const_iterator iter = attachments->begin();
       iter != attachments->end(); ++iter) {
    state->AddAttachment(iter->second);
  }

  AttachmentIdList::const_iterator iter = unavailable_attachment_ids->begin();
  AttachmentIdList::const_iterator end = unavailable_attachment_ids->end();
  if (result != AttachmentStore::STORE_INITIALIZATION_FAILED &&
      attachment_downloader_.get()) {
    // Try to download locally unavailable attachments.
    for (; iter != end; ++iter) {
      attachment_downloader_->DownloadAttachment(
          *iter, base::Bind(&AttachmentServiceImpl::DownloadDone,
                            weak_ptr_factory_.GetWeakPtr(), state, *iter));
    }
  } else {
    // No downloader so all locally unavailable attachments are unavailable.
    for (; iter != end; ++iter) {
      state->AddUnavailableAttachmentId(*iter);
    }
  }
}

void AttachmentServiceImpl::WriteDone(
    const scoped_refptr<GetOrDownloadState>& state,
    const Attachment& attachment,
    const AttachmentStore::Result& result) {
  switch (result) {
    case AttachmentStore::SUCCESS:
      state->AddAttachment(attachment);
      break;
    case AttachmentStore::UNSPECIFIED_ERROR:
    case AttachmentStore::STORE_INITIALIZATION_FAILED:
      state->AddUnavailableAttachmentId(attachment.GetId());
      break;
  }
}

void AttachmentServiceImpl::UploadDone(
    const AttachmentUploader::UploadResult& result,
    const AttachmentId& attachment_id) {
  DCHECK(CalledOnValidThread());
  AttachmentIdList ids;
  ids.push_back(attachment_id);
  switch (result) {
    case AttachmentUploader::UPLOAD_SUCCESS:
      attachment_store_->DropSyncReference(ids);
      upload_task_queue_->MarkAsSucceeded(attachment_id);
      if (delegate_) {
        delegate_->OnAttachmentUploaded(attachment_id);
      }
      break;
    case AttachmentUploader::UPLOAD_TRANSIENT_ERROR:
      upload_task_queue_->MarkAsFailed(attachment_id);
      upload_task_queue_->AddToQueue(attachment_id);
      break;
    case AttachmentUploader::UPLOAD_UNSPECIFIED_ERROR:
      // TODO(pavely): crbug/372622: Deal with UploadAttachment failures.
      attachment_store_->DropSyncReference(ids);
      upload_task_queue_->MarkAsFailed(attachment_id);
      break;
  }
}

void AttachmentServiceImpl::DownloadDone(
    const scoped_refptr<GetOrDownloadState>& state,
    const AttachmentId& attachment_id,
    const AttachmentDownloader::DownloadResult& result,
    std::unique_ptr<Attachment> attachment) {
  switch (result) {
    case AttachmentDownloader::DOWNLOAD_SUCCESS: {
      AttachmentList attachment_list;
      attachment_list.push_back(*attachment.get());
      attachment_store_->Write(
          attachment_list,
          base::Bind(&AttachmentServiceImpl::WriteDone,
                     weak_ptr_factory_.GetWeakPtr(), state, *attachment.get()));
      break;
    }
    case AttachmentDownloader::DOWNLOAD_TRANSIENT_ERROR:
    case AttachmentDownloader::DOWNLOAD_UNSPECIFIED_ERROR:
      state->AddUnavailableAttachmentId(attachment_id);
      break;
  }
}

void AttachmentServiceImpl::BeginUpload(const AttachmentId& attachment_id) {
  DCHECK(CalledOnValidThread());
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(attachment_id);
  attachment_store_->Read(attachment_ids,
                          base::Bind(&AttachmentServiceImpl::ReadDoneNowUpload,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void AttachmentServiceImpl::UploadAttachments(
    const AttachmentIdList& attachment_ids) {
  DCHECK(CalledOnValidThread());
  if (!attachment_uploader_.get()) {
    return;
  }
  attachment_store_->SetSyncReference(attachment_ids);

  for (auto iter = attachment_ids.begin(); iter != attachment_ids.end();
       ++iter) {
    upload_task_queue_->AddToQueue(*iter);
  }
}

void AttachmentServiceImpl::OnNetworkChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type != net::NetworkChangeNotifier::CONNECTION_NONE) {
    upload_task_queue_->ResetBackoff();
  }
}

void AttachmentServiceImpl::ReadDoneNowUpload(
    const AttachmentStore::Result& result,
    std::unique_ptr<AttachmentMap> attachments,
    std::unique_ptr<AttachmentIdList> unavailable_attachment_ids) {
  DCHECK(CalledOnValidThread());
  if (!unavailable_attachment_ids->empty()) {
    // TODO(maniscalco): We failed to read some attachments. What should we do
    // now?
    AttachmentIdList::const_iterator iter = unavailable_attachment_ids->begin();
    AttachmentIdList::const_iterator end = unavailable_attachment_ids->end();
    for (; iter != end; ++iter) {
      upload_task_queue_->Cancel(*iter);
    }
    attachment_store_->DropSyncReference(*unavailable_attachment_ids);
  }

  AttachmentMap::const_iterator iter = attachments->begin();
  AttachmentMap::const_iterator end = attachments->end();
  for (; iter != end; ++iter) {
    attachment_uploader_->UploadAttachment(
        iter->second, base::Bind(&AttachmentServiceImpl::UploadDone,
                                 weak_ptr_factory_.GetWeakPtr()));
  }
}

void AttachmentServiceImpl::SetTimerForTest(
    std::unique_ptr<base::Timer> timer) {
  upload_task_queue_->SetTimerForTest(std::move(timer));
}

}  // namespace syncer
