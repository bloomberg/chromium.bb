// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/strings/string_util.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/download_params.h"
#include "components/download/public/download_service.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"

BackgroundFetchDelegateImpl::BackgroundFetchDelegateImpl(Profile* profile)
    : download_service_(
          DownloadServiceFactory::GetInstance()->GetForBrowserContext(profile)),
      weak_ptr_factory_(this) {}

BackgroundFetchDelegateImpl::~BackgroundFetchDelegateImpl() {}

void BackgroundFetchDelegateImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (client()) {
    client()->OnDelegateShutdown();
  }
}

void BackgroundFetchDelegateImpl::DownloadUrl(
    const std::string& guid,
    const std::string& method,
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const net::HttpRequestHeaders& headers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  download::DownloadParams params;
  params.guid = guid;
  params.client = download::DownloadClient::BACKGROUND_FETCH;
  params.request_params.method = method;
  params.request_params.url = url;
  params.request_params.request_headers = headers;
  params.callback = base::Bind(&BackgroundFetchDelegateImpl::OnDownloadReceived,
                               weak_ptr_factory_.GetWeakPtr());
  params.traffic_annotation =
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation);

  download_service_->StartDownload(params);
}

void BackgroundFetchDelegateImpl::OnDownloadStarted(
    const std::string& guid,
    std::unique_ptr<content::BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (client())
    client()->OnDownloadStarted(guid, std::move(response));
}

void BackgroundFetchDelegateImpl::OnDownloadUpdated(const std::string& guid,
                                                    uint64_t bytes_downloaded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (client())
    client()->OnDownloadUpdated(guid, bytes_downloaded);
}

void BackgroundFetchDelegateImpl::OnDownloadFailed(
    const std::string& guid,
    download::Client::FailureReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using FailureReason = content::BackgroundFetchDelegate::FailureReason;
  FailureReason failure_reason;

  switch (reason) {
    case download::Client::FailureReason::NETWORK:
      failure_reason = FailureReason::NETWORK;
      break;
    case download::Client::FailureReason::TIMEDOUT:
      failure_reason = FailureReason::TIMEDOUT;
      break;
    case download::Client::FailureReason::UNKNOWN:
      failure_reason = FailureReason::UNKNOWN;
      break;

    case download::Client::FailureReason::ABORTED:
    case download::Client::FailureReason::CANCELLED:
      // The client cancelled or aborted it so no need to notify it.
      return;
    default:
      NOTREACHED();
      return;
  }

  if (client())
    client()->OnDownloadFailed(guid, failure_reason);
}

void BackgroundFetchDelegateImpl::OnDownloadSucceeded(
    const std::string& guid,
    const base::FilePath& path,
    uint64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (client()) {
    client()->OnDownloadComplete(
        guid, std::make_unique<content::BackgroundFetchResult>(
                  base::Time::Now(), path, size));
  }
}

void BackgroundFetchDelegateImpl::OnDownloadReceived(
    const std::string& guid,
    download::DownloadParams::StartResult result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using StartResult = download::DownloadParams::StartResult;
  switch (result) {
    case StartResult::ACCEPTED:
      // Nothing to do.
      break;
    case StartResult::BACKOFF:
      // TODO(delphick): try again later?
      // TODO(delphick): Due to a bug at the moment, this happens all the time
      // because successful downloads are not removed, so don't NOTREACHED.
      break;
    case StartResult::UNEXPECTED_CLIENT:
      // This really should never happen since we're supplying the
      // DownloadClient.
      NOTREACHED();
    case StartResult::UNEXPECTED_GUID:
      // TODO(delphick): try again with a different GUID.
      NOTREACHED();
    case StartResult::CLIENT_CANCELLED:
      // TODO(delphick): do we need to do anything here, since we will have
      // cancelled it?
      break;
    case StartResult::INTERNAL_ERROR:
      // TODO(delphick): We need to handle this gracefully.
      NOTREACHED();
    case StartResult::COUNT:
      NOTREACHED();
  }
}
