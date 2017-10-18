// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/download_params.h"
#include "components/download/public/download_service.h"
#include "components/offline_items_collection/core/offline_content_aggregator.h"
#include "components/offline_items_collection/core/offline_item.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"

BackgroundFetchDelegateImpl::BackgroundFetchDelegateImpl(Profile* profile)
    : download_service_(
          DownloadServiceFactory::GetInstance()->GetForBrowserContext(profile)),
      offline_content_aggregator_(
          offline_items_collection::OfflineContentAggregatorFactory::
              GetForBrowserContext(profile)),
      weak_ptr_factory_(this) {
  offline_content_aggregator_->RegisterProvider("background_fetch", this);
}

BackgroundFetchDelegateImpl::~BackgroundFetchDelegateImpl() {}

void BackgroundFetchDelegateImpl::Shutdown() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (client()) {
    client()->OnDelegateShutdown();
  }
}

BackgroundFetchDelegateImpl::JobDetails::JobDetails(JobDetails&&) = default;

BackgroundFetchDelegateImpl::JobDetails::JobDetails(
    const std::string& job_unique_id,
    const std::string& title,
    const url::Origin& origin,
    int completed_parts,
    int total_parts)
    : title(title),
      origin(origin),
      completed_parts(completed_parts),
      total_parts(total_parts),
      offline_item(offline_items_collection::ContentId("background_fetch",
                                                       job_unique_id)) {
  UpdateOfflineItem();
}

BackgroundFetchDelegateImpl::JobDetails::~JobDetails() = default;

void BackgroundFetchDelegateImpl::JobDetails::UpdateOfflineItem() {
  if (total_parts > 0) {
    offline_item.progress.value = completed_parts;
    offline_item.progress.max = total_parts;
    offline_item.progress.unit =
        offline_items_collection::OfflineItemProgressUnit::PERCENTAGE;
  }
  if (title.empty()) {
    offline_item.title = origin.Serialize();
  } else {
    // TODO(crbug.com/774612): Make sure that the origin is displayed completely
    // in all cases so that long titles cannot obscure it.
    offline_item.title = base::StringPrintf("%s (%s)", title.c_str(),
                                            origin.Serialize().c_str());
  }
  // TODO(delphick): Figure out what to put in offline_item.description.
  offline_item.is_transient = true;
  offline_item.state =
      (completed_parts == total_parts)
          ? offline_items_collection::OfflineItemState::COMPLETE
          : offline_items_collection::OfflineItemState::IN_PROGRESS;
}

void BackgroundFetchDelegateImpl::CreateDownloadJob(
    const std::string& job_unique_id,
    const std::string& title,
    const url::Origin& origin,
    int completed_parts,
    int total_parts,
    const std::vector<std::string>& current_guids) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(!job_details_map_.count(job_unique_id));

  auto emplace_result = job_details_map_.emplace(
      job_unique_id,
      JobDetails(job_unique_id, title, origin, completed_parts, total_parts));

  const JobDetails& details = emplace_result.first->second;

  for (const auto& download_guid : current_guids) {
    DCHECK(!download_job_unique_id_map_.count(download_guid));
    download_job_unique_id_map_.emplace(download_guid, job_unique_id);
  }

  for (auto* observer : observers_) {
    observer->OnItemsAdded({details.offline_item});
  }
}

void BackgroundFetchDelegateImpl::DownloadUrl(
    const std::string& job_unique_id,
    const std::string& download_guid,
    const std::string& method,
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const net::HttpRequestHeaders& headers) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DCHECK(job_details_map_.count(job_unique_id));
  DCHECK(!download_job_unique_id_map_.count(download_guid));

  JobDetails& job_details = job_details_map_.find(job_unique_id)->second;
  job_details.current_download_guids.insert(download_guid);

  download_job_unique_id_map_.emplace(download_guid, job_unique_id);

  download::DownloadParams params;
  params.guid = download_guid;
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
    const std::string& download_guid,
    std::unique_ptr<content::BackgroundFetchResponse> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (client())
    client()->OnDownloadStarted(download_guid, std::move(response));
}

void BackgroundFetchDelegateImpl::OnDownloadUpdated(
    const std::string& download_guid,
    uint64_t bytes_downloaded) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (client())
    client()->OnDownloadUpdated(download_guid, bytes_downloaded);
}

void BackgroundFetchDelegateImpl::OnDownloadFailed(
    const std::string& download_guid,
    download::Client::FailureReason reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  using FailureReason = content::BackgroundFetchResult::FailureReason;
  FailureReason failure_reason;

  const std::string& job_unique_id = download_job_unique_id_map_[download_guid];
  JobDetails& job_details = job_details_map_.find(job_unique_id)->second;

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

  // TODO(delphick): consider calling OnItemUpdated here as well if for instance
  // the download actually happened but 404ed.

  if (client()) {
    client()->OnDownloadComplete(
        download_guid, std::make_unique<content::BackgroundFetchResult>(
                           base::Time::Now(), failure_reason));
  }

  job_details.current_download_guids.erase(
      job_details.current_download_guids.find(download_guid));
  download_job_unique_id_map_.erase(download_guid);
}

void BackgroundFetchDelegateImpl::OnDownloadSucceeded(
    const std::string& download_guid,
    const base::FilePath& path,
    uint64_t size) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  const std::string& job_unique_id = download_job_unique_id_map_[download_guid];
  JobDetails& job_details = job_details_map_.find(job_unique_id)->second;
  ++job_details.completed_parts;
  job_details.UpdateOfflineItem();

  for (auto* observer : observers_)
    observer->OnItemUpdated(job_details.offline_item);

  if (client()) {
    client()->OnDownloadComplete(
        download_guid, std::make_unique<content::BackgroundFetchResult>(
                           base::Time::Now(), path, size));
  }

  job_details.current_download_guids.erase(
      job_details.current_download_guids.find(download_guid));
  download_job_unique_id_map_.erase(download_guid);
}

void BackgroundFetchDelegateImpl::OnDownloadReceived(
    const std::string& download_guid,
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

bool BackgroundFetchDelegateImpl::AreItemsAvailable() {
  return true;
}

void BackgroundFetchDelegateImpl::OpenItem(
    const offline_items_collection::ContentId& id) {
  // TODO(delphick): Add custom OpenItem behavior.
  NOTIMPLEMENTED();
}

void BackgroundFetchDelegateImpl::RemoveItem(
    const offline_items_collection::ContentId& id) {
  // TODO(delphick): Support removing items. (Not sure when this would actually
  // get called though).
  NOTIMPLEMENTED();
}

void BackgroundFetchDelegateImpl::CancelDownload(
    const offline_items_collection::ContentId& id) {
  auto job_details_iter = job_details_map_.find(id.id);
  if (job_details_iter == job_details_map_.end())
    return;

  JobDetails& job_details = job_details_iter->second;

  for (auto& download_guid : job_details.current_download_guids) {
    download_service_->CancelDownload(download_guid);
    if (client()) {
      client()->OnDownloadComplete(
          download_guid,
          std::make_unique<content::BackgroundFetchResult>(
              base::Time::Now(),
              content::BackgroundFetchResult::FailureReason::CANCELLED));
    }
    download_job_unique_id_map_.erase(download_guid);
  }

  job_details_map_.erase(job_details_iter);
}

void BackgroundFetchDelegateImpl::PauseDownload(
    const offline_items_collection::ContentId& id) {
  auto job_details_iter = job_details_map_.find(id.id);
  if (job_details_iter == job_details_map_.end())
    return;

  JobDetails& job_details = job_details_iter->second;
  for (auto& download_guid : job_details.current_download_guids)
    download_service_->PauseDownload(download_guid);

  // TODO(delphick): Mark overall download job as paused so that future
  // downloads are not started until resume. (Initially not a worry because only
  // one download will be scheduled at a time).
}

void BackgroundFetchDelegateImpl::ResumeDownload(
    const offline_items_collection::ContentId& id,
    bool has_user_gesture) {
  auto job_details_iter = job_details_map_.find(id.id);
  if (job_details_iter == job_details_map_.end())
    return;

  JobDetails& job_details = job_details_iter->second;
  for (auto& download_guid : job_details.current_download_guids)
    download_service_->ResumeDownload(download_guid);

  // TODO(delphick): Start new downloads that weren't started because of pause.
}

const offline_items_collection::OfflineItem*
BackgroundFetchDelegateImpl::GetItemById(
    const offline_items_collection::ContentId& id) {
  auto it = job_details_map_.find(id.id);
  return (it != job_details_map_.end()) ? &it->second.offline_item : nullptr;
}

BackgroundFetchDelegateImpl::OfflineItemList
BackgroundFetchDelegateImpl::GetAllItems() {
  OfflineItemList item_list;
  for (auto& entry : job_details_map_)
    item_list.push_back(entry.second.offline_item);
  return item_list;
}

void BackgroundFetchDelegateImpl::GetVisualsForItem(
    const offline_items_collection::ContentId& id,
    const VisualsCallback& callback) {
  // GetVisualsForItem mustn't be called directly since offline_items_collection
  // is not re-entrant and it must be called even if there are no visuals.
  // TODO(delphick): Call with an image when that becomes available.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, id, nullptr));
}

void BackgroundFetchDelegateImpl::AddObserver(Observer* observer) {
  DCHECK(!observers_.count(observer));

  observers_.insert(observer);
  // OnItemsAvailable mustn't be called directly since offline_items_collection
  // is not re-entrant.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](Observer* observer,
                        base::WeakPtr<BackgroundFetchDelegateImpl> provider) {
                       if (provider)
                         observer->OnItemsAvailable(provider.get());
                     },
                     observer, weak_ptr_factory_.GetWeakPtr()));
}

void BackgroundFetchDelegateImpl::RemoveObserver(Observer* observer) {
  observers_.erase(observer);
}
