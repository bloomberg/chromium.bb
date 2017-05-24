// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/download_driver_impl.h"

#include "components/download/internal/driver_entry.h"
#include "content/public/browser/download_interrupt_reasons.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/storage_partition.h"
#include "net/http/http_response_headers.h"

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

}  // namespace

// static
DriverEntry DownloadDriverImpl::CreateDriverEntry(
    const content::DownloadItem* item) {
  DCHECK(item);
  DriverEntry entry;
  entry.guid = item->GetGuid();
  entry.state = ToDriverEntryState(item->GetState());
  entry.paused = item->IsPaused();
  entry.bytes_downloaded = item->GetReceivedBytes();
  entry.expected_total_size = item->GetTotalBytes();
  entry.response_headers = item->GetResponseHeaders();
  return entry;
}

DownloadDriverImpl::DownloadDriverImpl(content::DownloadManager* manager,
                                       const base::FilePath& dir)
    : download_manager_(manager), file_dir_(dir), client_(nullptr) {
  DCHECK(download_manager_);
}

DownloadDriverImpl::~DownloadDriverImpl() {
  if (download_manager_)
    download_manager_->RemoveObserver(this);
}

void DownloadDriverImpl::Initialize(DownloadDriver::Client* client) {
  DCHECK(!client_);
  client_ = client;
  DCHECK(client_);

  // |download_manager_| may be shut down. Informs the client.
  if (!download_manager_) {
    client_->OnDriverReady(false);
    return;
  }

  download_manager_->AddObserver(this);
  if (download_manager_->IsManagerInitialized())
    client_->OnDriverReady(true);
}

bool DownloadDriverImpl::IsReady() const {
  return client_ && download_manager_;
}

void DownloadDriverImpl::Start(const DownloadParams& params) {
  DCHECK(!params.request_params.url.is_empty());
  DCHECK(!params.guid.empty());
  if (!download_manager_)
    return;

  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(
          download_manager_->GetBrowserContext(), params.request_params.url);
  DCHECK(storage_partition);

  std::unique_ptr<content::DownloadUrlParameters> download_url_params(
      new content::DownloadUrlParameters(
          params.request_params.url,
          storage_partition->GetURLRequestContext()));

  // TODO(xingliu): Handle the request headers from |params|, need to tweak
  // download network stack.
  // Make content::DownloadManager handle potential guid collision and return
  // an error to fail the download cleanly.
  download_url_params->set_guid(params.guid);
  download_url_params->set_transient(true);
  download_url_params->set_method(params.request_params.method);
  download_url_params->set_file_path(file_dir_.AppendASCII(params.guid));

  download_manager_->DownloadUrl(std::move(download_url_params));
}

void DownloadDriverImpl::Cancel(const std::string& guid) {
  if (!download_manager_)
    return;
  content::DownloadItem* item = download_manager_->GetDownloadByGuid(guid);
  // Cancels the download and removes the persisted records in content layer.
  if (item) {
    item->RemoveObserver(this);
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

void DownloadDriverImpl::OnDownloadUpdated(content::DownloadItem* item) {
  DCHECK(client_);

  using DownloadState = content::DownloadItem::DownloadState;
  DownloadState state = item->GetState();
  content::DownloadInterruptReason reason = item->GetLastReason();
  DriverEntry entry = CreateDriverEntry(item);

  if (state == DownloadState::COMPLETE) {
    client_->OnDownloadSucceeded(entry, item->GetTargetFilePath());
    item->RemoveObserver(this);
  } else if (state == DownloadState::IN_PROGRESS) {
    client_->OnDownloadUpdated(entry);
  } else if (reason !=
             content::DownloadInterruptReason::DOWNLOAD_INTERRUPT_REASON_NONE) {
    client_->OnDownloadFailed(entry, static_cast<int>(reason));
    item->RemoveObserver(this);
  }
}

void DownloadDriverImpl::OnDownloadCreated(content::DownloadManager* manager,
                                           content::DownloadItem* item) {
  // Listens to all downloads.
  item->AddObserver(this);
  DCHECK(client_);
  DriverEntry entry = CreateDriverEntry(item);
  client_->OnDownloadCreated(entry);
}

void DownloadDriverImpl::OnManagerInitialized() {
  DCHECK(client_);
  DCHECK(download_manager_);
  client_->OnDriverReady(true);
}

void DownloadDriverImpl::ManagerGoingDown(content::DownloadManager* manager) {
  DCHECK_EQ(download_manager_, manager);
  download_manager_ = nullptr;
}

}  // namespace download
