// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_download_client.h"

#include <memory>
#include <set>
#include <utility>

#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"
#include "chrome/browser/download/download_service_factory.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/download_service.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_context.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "url/origin.h"

BackgroundFetchDownloadClient::BackgroundFetchDownloadClient(
    content::BrowserContext* context)
    : browser_context_(context), delegate_(nullptr) {}

BackgroundFetchDownloadClient::~BackgroundFetchDownloadClient() = default;

void BackgroundFetchDownloadClient::OnServiceInitialized(
    bool state_lost,
    const std::vector<download::DownloadMetaData>& downloads) {
  std::set<std::string> outstanding_guids =
      GetDelegate()->TakeOutstandingGuids();
  for (const auto& download : downloads) {
    if (!outstanding_guids.count(download.guid)) {
      // Background Fetch is not aware of this GUID, so it successfully
      // completed but the information is still around.
      continue;
    }

    if (download.completion_info) {
      // The download finished but was not persisted.
      OnDownloadSucceeded(download.guid, *download.completion_info);
    }

    // The download is resuming, and will call the appropriate functions.
  }

  // There is also the case that the Download Service is not aware of the GUID.
  // i.e. there is a guid in |outstanding_guids| not in |downloads|.
  // This can be due to:
  // 1. The browser crashing before the download started.
  // 2. The download failing before persisting the state.
  // 3. The browser was forced to clean-up the the download.
  // In either case the download should be allowed to restart, so there is
  // nothing to do here.
}

void BackgroundFetchDownloadClient::OnServiceUnavailable() {}

download::Client::ShouldDownload
BackgroundFetchDownloadClient::OnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {
  std::unique_ptr<content::BackgroundFetchResponse> response =
      std::make_unique<content::BackgroundFetchResponse>(url_chain, headers);
  GetDelegate()->OnDownloadStarted(guid, std::move(response));

  // TODO(delphick): validate the chain/headers before returning CONTINUE
  return download::Client::ShouldDownload::CONTINUE;
}

void BackgroundFetchDownloadClient::OnDownloadUpdated(
    const std::string& guid,
    uint64_t bytes_downloaded) {
  GetDelegate()->OnDownloadUpdated(guid, bytes_downloaded);
}

void BackgroundFetchDownloadClient::OnDownloadFailed(
    const std::string& guid,
    const download::CompletionInfo& info,
    download::Client::FailureReason reason) {
  GetDelegate()->OnDownloadFailed(guid, reason);
}

void BackgroundFetchDownloadClient::OnDownloadSucceeded(
    const std::string& guid,
    const download::CompletionInfo& info) {
  // Pass the response headers and url_chain to the client again, in case
  // this fetch was restarted and the client has lost this info.
  // TODO(crbug.com/884672): Move CORS checks to `OnDownloadStarted`,
  // and pass all the completion info to the client once.
  OnDownloadStarted(guid, info.url_chain, info.response_headers);
  GetDelegate()->OnDownloadSucceeded(guid, info.path, info.blob_handle,
                                     info.bytes_downloaded);
}

bool BackgroundFetchDownloadClient::CanServiceRemoveDownloadedFile(
    const std::string& guid,
    bool force_delete) {
  // If |force_delete| is true the file will be removed anyway.
  // TODO(rayankans): Add UMA to see how often this happens.
  return force_delete || GetDelegate()->IsGuidOutstanding(guid);
}

void BackgroundFetchDownloadClient::GetUploadData(
    const std::string& guid,
    download::GetUploadDataCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), nullptr));
}

BackgroundFetchDelegateImpl* BackgroundFetchDownloadClient::GetDelegate() {
  if (delegate_)
    return delegate_.get();

  content::BackgroundFetchDelegate* delegate =
      browser_context_->GetBackgroundFetchDelegate();

  delegate_ = static_cast<BackgroundFetchDelegateImpl*>(delegate)->GetWeakPtr();
  DCHECK(delegate_);
  return delegate_.get();
}
