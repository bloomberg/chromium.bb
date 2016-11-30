// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_download_service.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "components/reading_list/ios/reading_list_entry.h"
#include "components/reading_list/ios/reading_list_model.h"
#include "ios/web/public/web_thread.h"

namespace {
// Number of time the download must fail before the download occurs only in
// wifi.
const int kNumberOfFailsBeforeWifiOnly = 5;
// Number of time the download must fail before we give up trying to download
// it.
const int kNumberOfFailsBeforeStop = 7;
}  // namespace

ReadingListDownloadService::ReadingListDownloadService(
    ReadingListModel* reading_list_model,
    dom_distiller::DomDistillerService* distiller_service,
    PrefService* prefs,
    base::FilePath chrome_profile_path)
    : reading_list_model_(reading_list_model),
      had_connection_(!net::NetworkChangeNotifier::IsOffline()),
      weak_ptr_factory_(this) {
  DCHECK(reading_list_model);
  url_downloader_ = base::MakeUnique<URLDownloader>(
      distiller_service, prefs, chrome_profile_path,
      base::Bind(&ReadingListDownloadService::OnDownloadEnd,
                 base::Unretained(this)),
      base::Bind(&ReadingListDownloadService::OnDeleteEnd,
                 base::Unretained(this)));
  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

ReadingListDownloadService::~ReadingListDownloadService() {
  net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void ReadingListDownloadService::Initialize() {
  reading_list_model_->AddObserver(this);
}

void ReadingListDownloadService::Shutdown() {
  reading_list_model_->RemoveObserver(this);
}

void ReadingListDownloadService::ReadingListModelLoaded(
    const ReadingListModel* model) {
  DCHECK_EQ(reading_list_model_, model);
  DownloadAllEntries();
}

void ReadingListDownloadService::ReadingListWillRemoveEntry(
    const ReadingListModel* model,
    const GURL& url) {
  DCHECK_EQ(reading_list_model_, model);
  DCHECK(model->GetEntryByURL(url));
  RemoveDownloadedEntry(url);
}

void ReadingListDownloadService::ReadingListDidAddEntry(
    const ReadingListModel* model,
    const GURL& url) {
  DCHECK_EQ(reading_list_model_, model);
  ScheduleDownloadEntry(url);
}

void ReadingListDownloadService::DownloadAllEntries() {
  DCHECK(reading_list_model_->loaded());
  for (const auto& url : reading_list_model_->Keys()) {
    this->ScheduleDownloadEntry(url);
  }
}

void ReadingListDownloadService::ScheduleDownloadEntry(const GURL& url) {
  DCHECK(reading_list_model_->loaded());
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  if (!entry || entry->DistilledState() == ReadingListEntry::ERROR ||
      entry->DistilledState() == ReadingListEntry::PROCESSED)
    return;
  GURL local_url(url);
  web::WebThread::PostDelayedTask(
      web::WebThread::UI, FROM_HERE,
      base::Bind(&ReadingListDownloadService::DownloadEntry,
                 weak_ptr_factory_.GetWeakPtr(), local_url),
      entry->TimeUntilNextTry());
}

void ReadingListDownloadService::DownloadEntry(const GURL& url) {
  DCHECK(reading_list_model_->loaded());
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  if (!entry || entry->DistilledState() == ReadingListEntry::ERROR ||
      entry->DistilledState() == ReadingListEntry::PROCESSED)
    return;

  if (net::NetworkChangeNotifier::IsOffline()) {
    // There is no connection, save it for download only if we did not exceed
    // the maximaxum number of tries.
    if (entry->FailedDownloadCounter() < kNumberOfFailsBeforeWifiOnly)
      url_to_download_cellular_.push_back(entry->URL());
    if (entry->FailedDownloadCounter() < kNumberOfFailsBeforeStop)
      url_to_download_wifi_.push_back(entry->URL());
    return;
  }

  // There is a connection.
  if (entry->FailedDownloadCounter() < kNumberOfFailsBeforeWifiOnly) {
    // Try to download the page, whatever the connection.
    reading_list_model_->SetEntryDistilledState(entry->URL(),
                                                ReadingListEntry::PROCESSING);
    url_downloader_->DownloadOfflineURL(entry->URL());

  } else if (entry->FailedDownloadCounter() < kNumberOfFailsBeforeStop) {
    // Try to download the page only if the connection is wifi.
    if (net::NetworkChangeNotifier::GetConnectionType() ==
        net::NetworkChangeNotifier::CONNECTION_WIFI) {
      // The connection is wifi, download the page.
      reading_list_model_->SetEntryDistilledState(entry->URL(),
                                                  ReadingListEntry::PROCESSING);
      url_downloader_->DownloadOfflineURL(entry->URL());

    } else {
      // The connection is not wifi, save it for download when the connection
      // changes to wifi.
      url_to_download_wifi_.push_back(entry->URL());
    }
  }
}

void ReadingListDownloadService::RemoveDownloadedEntry(const GURL& url) {
  DCHECK(reading_list_model_->loaded());
  url_downloader_->RemoveOfflineURL(url);
}

void ReadingListDownloadService::OnDownloadEnd(
    const GURL& url,
    URLDownloader::SuccessState success,
    const base::FilePath& distilled_path,
    const std::string& title) {
  DCHECK(reading_list_model_->loaded());
  if ((success == URLDownloader::DOWNLOAD_SUCCESS ||
       success == URLDownloader::DOWNLOAD_EXISTS) &&
      !distilled_path.empty()) {
    reading_list_model_->SetEntryDistilledPath(url, distilled_path);

  } else if (success == URLDownloader::ERROR_RETRY) {
    reading_list_model_->SetEntryDistilledState(url,
                                                ReadingListEntry::WILL_RETRY);
    ScheduleDownloadEntry(url);
  } else if (success == URLDownloader::ERROR_PERMANENT) {
    reading_list_model_->SetEntryDistilledState(url, ReadingListEntry::ERROR);
  }
}

void ReadingListDownloadService::OnDeleteEnd(const GURL& url, bool success) {
  // Nothing to update as this is only called when deleting reading list entries
}

void ReadingListDownloadService::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  if (type == net::NetworkChangeNotifier::CONNECTION_NONE) {
    had_connection_ = false;
    return;
  }

  if (!had_connection_) {
    had_connection_ = true;
    for (auto& url : url_to_download_cellular_) {
      ScheduleDownloadEntry(url);
    }
  }
  if (type == net::NetworkChangeNotifier::CONNECTION_WIFI) {
    for (auto& url : url_to_download_wifi_) {
      ScheduleDownloadEntry(url);
    }
  }
}
