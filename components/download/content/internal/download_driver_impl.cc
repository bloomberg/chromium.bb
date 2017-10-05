// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/internal/download_driver_impl.h"

#include <set>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/download/internal/driver_entry.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"

namespace download {

namespace {

// Converts a content::DownloadItem::DownloadState to DriverEntry::State.
DriverEntry::State ToDriverEntryState(
    content::DownloadItem::DownloadState state) {
  switch (state) {
    case content::DownloadItem::IN_PROGRESS:
      return DriverEntry::State::IN_PROGRESS;
    case content::DownloadItem::COMPLETE:
      return DriverEntry::State::COMPLETE;
    case content::DownloadItem::CANCELLED:
      return DriverEntry::State::CANCELLED;
    case content::DownloadItem::INTERRUPTED:
      return DriverEntry::State::INTERRUPTED;
    case content::DownloadItem::MAX_DOWNLOAD_STATE:
      return DriverEntry::State::UNKNOWN;
    default:
      NOTREACHED();
      return DriverEntry::State::UNKNOWN;
  }
}

FailureType FailureTypeFromInterruptReason(
    content::DownloadInterruptReason reason) {
  switch (reason) {
    case content::DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED:
    case content::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE:
    case content::DOWNLOAD_INTERRUPT_REASON_FILE_NAME_TOO_LONG:
    case content::DOWNLOAD_INTERRUPT_REASON_FILE_TOO_LARGE:
    case content::DOWNLOAD_INTERRUPT_REASON_FILE_VIRUS_INFECTED:
    case content::DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED:
    case content::DOWNLOAD_INTERRUPT_REASON_FILE_SECURITY_CHECK_FAILED:
    case content::DOWNLOAD_INTERRUPT_REASON_SERVER_UNAUTHORIZED:
    case content::DOWNLOAD_INTERRUPT_REASON_SERVER_CERT_PROBLEM:
    case content::DOWNLOAD_INTERRUPT_REASON_SERVER_FORBIDDEN:
    case content::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED:
      return FailureType::NOT_RECOVERABLE;
    default:
      return FailureType::RECOVERABLE;
  }
}

// Logs interrupt reason when download fails.
void LogDownloadInterruptReason(content::DownloadInterruptReason reason) {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Download.Service.Driver.InterruptReason",
                              reason);
}

}  // namespace

// static
DriverEntry DownloadDriverImpl::CreateDriverEntry(
    const content::DownloadItem* item) {
  DCHECK(item);
  DriverEntry entry;
  entry.guid = item->GetGuid();
  entry.state = ToDriverEntryState(item->GetState());
  entry.paused = item->IsPaused();
  entry.done = item->IsDone();
  entry.bytes_downloaded = item->GetReceivedBytes();
  entry.expected_total_size = item->GetTotalBytes();
  entry.current_file_path =
      item->GetState() == content::DownloadItem::DownloadState::COMPLETE
          ? item->GetTargetFilePath()
          : item->GetFullPath();
  entry.completion_time = item->GetEndTime();
  entry.response_headers = item->GetResponseHeaders();
  if (entry.response_headers.get()) {
    entry.can_resume =
        entry.response_headers->HasHeaderValue("Accept-Ranges", "bytes") ||
        (entry.response_headers->HasHeader("Content-Range") &&
         entry.response_headers->response_code() == net::HTTP_PARTIAL_CONTENT);
    entry.can_resume &= entry.response_headers->HasStrongValidators();
  } else {
    // If we haven't issued the request yet, treat this like a resume based on
    // the etag and last modified time.
    entry.can_resume =
        !item->GetETag().empty() || !item->GetLastModifiedTime().empty();
  }
  entry.url_chain = item->GetUrlChain();
  return entry;
}

DownloadDriverImpl::DownloadDriverImpl(content::DownloadManager* manager)
    : download_manager_(manager), client_(nullptr), weak_ptr_factory_(this) {
  DCHECK(download_manager_);
}

DownloadDriverImpl::~DownloadDriverImpl() = default;

void DownloadDriverImpl::Initialize(DownloadDriver::Client* client) {
  DCHECK(!client_);
  client_ = client;
  DCHECK(client_);

  // |download_manager_| may be shut down. Informs the client.
  if (!download_manager_) {
    client_->OnDriverReady(false);
    return;
  }

  notifier_ =
      base::MakeUnique<AllDownloadItemNotifier>(download_manager_, this);
}

void DownloadDriverImpl::HardRecover() {
  // TODO(dtrainor, xingliu): Implement recovery for the DownloadManager.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DownloadDriverImpl::OnHardRecoverComplete,
                            weak_ptr_factory_.GetWeakPtr(), true));
}

bool DownloadDriverImpl::IsReady() const {
  return client_ && download_manager_ &&
         download_manager_->IsManagerInitialized();
}

void DownloadDriverImpl::Start(
    const RequestParams& request_params,
    const std::string& guid,
    const base::FilePath& file_path,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(!request_params.url.is_empty());
  DCHECK(!guid.empty());
  if (!download_manager_)
    return;

  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(
          download_manager_->GetBrowserContext(), request_params.url);
  DCHECK(storage_partition);

  std::unique_ptr<content::DownloadUrlParameters> download_url_params(
      new content::DownloadUrlParameters(
          request_params.url, storage_partition->GetURLRequestContext(),
          traffic_annotation));

  // TODO(xingliu): Make content::DownloadManager handle potential guid
  // collision and return an error to fail the download cleanly.
  for (net::HttpRequestHeaders::Iterator it(request_params.request_headers);
       it.GetNext();) {
    download_url_params->add_request_header(it.name(), it.value());
  }
  download_url_params->set_guid(guid);
  download_url_params->set_transient(true);
  download_url_params->set_method(request_params.method);
  download_url_params->set_file_path(file_path);

  download_manager_->DownloadUrl(std::move(download_url_params));
}

void DownloadDriverImpl::Remove(const std::string& guid) {
  guid_to_remove_.emplace(guid);

  // DownloadItem::Remove will cause the item object removed from memory, post
  // the remove task to avoid the object being accessed in the same call stack.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&DownloadDriverImpl::DoRemoveDownload,
                            weak_ptr_factory_.GetWeakPtr(), guid));
}

void DownloadDriverImpl::DoRemoveDownload(const std::string& guid) {
  if (!download_manager_)
    return;
  content::DownloadItem* item = download_manager_->GetDownloadByGuid(guid);
  // Cancels the download and removes the persisted records in content layer.
  if (item) {
    item->Remove();
  }
}

void DownloadDriverImpl::Pause(const std::string& guid) {
  if (!download_manager_)
    return;
  content::DownloadItem* item = download_manager_->GetDownloadByGuid(guid);
  if (item)
    item->Pause();
}

void DownloadDriverImpl::Resume(const std::string& guid) {
  if (!download_manager_)
    return;
  content::DownloadItem* item = download_manager_->GetDownloadByGuid(guid);
  if (item)
    item->Resume();
}

base::Optional<DriverEntry> DownloadDriverImpl::Find(const std::string& guid) {
  if (!download_manager_)
    return base::nullopt;
  content::DownloadItem* item = download_manager_->GetDownloadByGuid(guid);
  if (item)
    return CreateDriverEntry(item);
  return base::nullopt;
}

std::set<std::string> DownloadDriverImpl::GetActiveDownloads() {
  std::set<std::string> guids;
  if (!download_manager_)
    return guids;

  std::vector<content::DownloadItem*> items;
  download_manager_->GetAllDownloads(&items);

  for (auto* item : items) {
    DriverEntry::State state = ToDriverEntryState(item->GetState());
    if (state == DriverEntry::State::IN_PROGRESS)
      guids.insert(item->GetGuid());
  }

  return guids;
}

void DownloadDriverImpl::OnDownloadUpdated(content::DownloadManager* manager,
                                           content::DownloadItem* item) {
  DCHECK(client_);
  // Blocks the observer call if we asked to remove the download.
  if (guid_to_remove_.find(item->GetGuid()) != guid_to_remove_.end())
    return;

  using DownloadState = content::DownloadItem::DownloadState;
  DownloadState state = item->GetState();
  content::DownloadInterruptReason reason = item->GetLastReason();
  DriverEntry entry = CreateDriverEntry(item);

  if (state == DownloadState::COMPLETE) {
    client_->OnDownloadSucceeded(entry);
  } else if (state == DownloadState::IN_PROGRESS) {
    client_->OnDownloadUpdated(entry);
  } else if (reason !=
             content::DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE) {
    if (client_->IsTrackingDownload(item->GetGuid()))
      LogDownloadInterruptReason(reason);
    client_->OnDownloadFailed(entry, FailureTypeFromInterruptReason(reason));
  }
}

void DownloadDriverImpl::OnDownloadRemoved(content::DownloadManager* manager,
                                           content::DownloadItem* download) {
  guid_to_remove_.erase(download->GetGuid());
  // |download| is about to be deleted.
}

void DownloadDriverImpl::OnDownloadCreated(content::DownloadManager* manager,
                                           content::DownloadItem* item) {
  // Listens to all downloads.
  DCHECK(client_);
  DriverEntry entry = CreateDriverEntry(item);

  // Only notifies the client about new downloads. Existing download data will
  // be loaded before the driver is ready.
  if (IsReady())
    client_->OnDownloadCreated(entry);
}

void DownloadDriverImpl::OnManagerInitialized(
    content::DownloadManager* manager) {
  DCHECK_EQ(download_manager_, manager);
  DCHECK(client_);
  DCHECK(download_manager_);
  client_->OnDriverReady(true);
}

void DownloadDriverImpl::OnManagerGoingDown(content::DownloadManager* manager) {
  DCHECK_EQ(download_manager_, manager);
  download_manager_ = nullptr;
}

void DownloadDriverImpl::OnHardRecoverComplete(bool success) {
  client_->OnDriverHardRecoverComplete(success);
}

}  // namespace download
