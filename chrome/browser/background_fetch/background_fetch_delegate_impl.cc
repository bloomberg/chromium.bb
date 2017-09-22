// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"

#include <utility>

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/browser/storage_partition.h"

#if defined(OS_ANDROID)
#include "base/android/path_utils.h"
#include "base/files/file_path.h"
#endif

namespace {

class DownloadItemObserver : public content::DownloadItem::Observer {
 public:
  explicit DownloadItemObserver(
      base::WeakPtr<content::BackgroundFetchDelegate::Client> client)
      : client_(client) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  }

  void OnDownloadUpdated(content::DownloadItem* download_item) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    if (!client_.get()) {
      download_item->RemoveObserver(this);
      delete this;
      return;
    }

    switch (download_item->GetState()) {
      case content::DownloadItem::DownloadState::COMPLETE:
        client_->OnDownloadComplete(
            download_item->GetGuid(),
            std::make_unique<content::BackgroundFetchResult>(
                download_item->GetEndTime(), download_item->GetTargetFilePath(),
                download_item->GetReceivedBytes()));
        download_item->RemoveObserver(this);
        delete this;
        // Cannot access this after deleting itself so return immediately.
        return;
      case content::DownloadItem::DownloadState::CANCELLED:
        // TODO(delphick): Consider how we want to handle cancelled downloads.
        break;
      case content::DownloadItem::DownloadState::INTERRUPTED:
        // TODO(delphick): Just update the notification that it is paused.
        break;
      case content::DownloadItem::DownloadState::IN_PROGRESS:
        // TODO(delphick): If the download was previously paused, this should
        // now unpause the notification.
        break;
      case content::DownloadItem::DownloadState::MAX_DOWNLOAD_STATE:
        NOTREACHED();
        break;
    }
  }

  void OnDownloadDestroyed(content::DownloadItem* download_item) override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    download_item->RemoveObserver(this);
    delete this;
  }

 private:
  base::WeakPtr<content::BackgroundFetchDelegate::Client> client_;
};

#if defined(OS_ANDROID)
// Prefix for files stored in the Chromium-internal download directory to
// indicate files that were fetched through Background Fetch.
const char kBackgroundFetchFilePrefix[] = "BGFetch-";
#endif  // defined(OS_ANDROID)

}  // namespace

BackgroundFetchDelegateImpl::BackgroundFetchDelegateImpl(Profile* profile)
    : profile_(profile), weak_ptr_factory_(this) {}

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

  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(profile_);
  DCHECK(download_manager);

  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartitionForSite(profile_, url);

  std::unique_ptr<content::DownloadUrlParameters> download_parameters(
      std::make_unique<content::DownloadUrlParameters>(
          url, storage_partition->GetURLRequestContext(), traffic_annotation));

  net::HttpRequestHeaders::Iterator iterator(headers);
  while (iterator.GetNext())
    download_parameters->add_request_header(iterator.name(), iterator.value());

  // TODO(peter): Background Fetch responses should not end up in the user's
  // download folder on any platform. Find an appropriate solution for desktop
  // too. The Android internal directory is not scoped to a profile.

  download_parameters->set_transient(true);

#if defined(OS_ANDROID)
  base::FilePath download_directory;
  if (base::android::GetDownloadInternalDirectory(&download_directory)) {
    download_parameters->set_file_path(download_directory.Append(
        std::string(kBackgroundFetchFilePrefix) + guid));
  }
#endif  // defined(OS_ANDROID)

  download_parameters->set_callback(
      base::Bind(&BackgroundFetchDelegateImpl::DidStartRequest, GetWeakPtr()));
  download_parameters->set_guid(guid);

  download_manager->DownloadUrl(std::move(download_parameters));
}

base::WeakPtr<BackgroundFetchDelegateImpl>
BackgroundFetchDelegateImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void BackgroundFetchDelegateImpl::DidStartRequest(
    content::DownloadItem* download_item,
    content::DownloadInterruptReason interrupt_reason) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(peter): These two DCHECKs are assumptions our implementation
  // currently makes, but are not fit for production. We need to handle such
  // failures gracefully.
  DCHECK_EQ(interrupt_reason, content::DOWNLOAD_INTERRUPT_REASON_NONE);
  DCHECK(download_item);

  // Register for updates on the download's progress.
  download_item->AddObserver(new DownloadItemObserver(client()));

  client()->OnDownloadStarted(
      download_item->GetGuid(),
      std::make_unique<content::BackgroundFetchResponse>(
          download_item->GetUrlChain(), download_item->GetResponseHeaders()));
}
