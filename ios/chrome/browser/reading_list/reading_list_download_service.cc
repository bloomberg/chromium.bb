// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_download_service.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "components/reading_list/reading_list_entry.h"
#include "components/reading_list/reading_list_model.h"
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

void ReadingListDownloadService::ReadingListWillRemoveReadEntry(
    const ReadingListModel* model,
    size_t index) {
  DCHECK_EQ(reading_list_model_, model);
  RemoveDownloadedEntry(model->GetReadEntryAtIndex(index));
}

void ReadingListDownloadService::ReadingListWillRemoveUnreadEntry(
    const ReadingListModel* model,
    size_t index) {
  DCHECK_EQ(reading_list_model_, model);
  RemoveDownloadedEntry(model->GetUnreadEntryAtIndex(index));
}

void ReadingListDownloadService::ReadingListWillAddUnreadEntry(
    const ReadingListModel* model,
    const ReadingListEntry& entry) {
  DCHECK_EQ(reading_list_model_, model);
  ScheduleDownloadEntry(entry);
}

void ReadingListDownloadService::ReadingListWillAddReadEntry(
    const ReadingListModel* model,
    const ReadingListEntry& entry) {
  DCHECK_EQ(reading_list_model_, model);
  ScheduleDownloadEntry(entry);
}

void ReadingListDownloadService::DownloadAllEntries() {
  DCHECK(reading_list_model_->loaded());
  size_t size = reading_list_model_->unread_size();
  for (size_t i = 0; i < size; i++) {
    const ReadingListEntry& entry =
        reading_list_model_->GetUnreadEntryAtIndex(i);
    this->ScheduleDownloadEntry(entry);
  }
  size = reading_list_model_->read_size();
  for (size_t i = 0; i < size; i++) {
    const ReadingListEntry& entry = reading_list_model_->GetReadEntryAtIndex(i);
    this->ScheduleDownloadEntry(entry);
  }
}

void ReadingListDownloadService::ScheduleDownloadEntry(
    const ReadingListEntry& entry) {
  DCHECK(reading_list_model_->loaded());
  if (entry.DistilledState() == ReadingListEntry::ERROR ||
      entry.DistilledState() == ReadingListEntry::PROCESSED)
    return;

  web::WebThread::PostDelayedTask(
      web::WebThread::UI, FROM_HERE,
      base::Bind(&ReadingListDownloadService::DownloadEntryFromURL,
                 weak_ptr_factory_.GetWeakPtr(), entry.URL()),
      entry.TimeUntilNextTry());
}

void ReadingListDownloadService::ScheduleDownloadEntryFromURL(const GURL& url) {
  auto download_callback =
      base::Bind(&ReadingListDownloadService::ScheduleDownloadEntry,
                 base::Unretained(this));
  reading_list_model_->CallbackEntryURL(url, download_callback);
}

void ReadingListDownloadService::DownloadEntryFromURL(const GURL& url) {
  auto download_callback = base::Bind(
      &ReadingListDownloadService::DownloadEntry, base::Unretained(this));
  reading_list_model_->CallbackEntryURL(url, download_callback);
}

void ReadingListDownloadService::DownloadEntry(const ReadingListEntry& entry) {
  DCHECK(reading_list_model_->loaded());
  if (entry.DistilledState() == ReadingListEntry::ERROR ||
      entry.DistilledState() == ReadingListEntry::PROCESSED)
    return;

  if (net::NetworkChangeNotifier::IsOffline()) {
    // There is no connection, save it for download only if we did not exceed
    // the maximaxum number of tries.
    if (entry.FailedDownloadCounter() < kNumberOfFailsBeforeWifiOnly)
      url_to_download_cellular_.push_back(entry.URL());
    if (entry.FailedDownloadCounter() < kNumberOfFailsBeforeStop)
      url_to_download_wifi_.push_back(entry.URL());
    return;
  }

  // There is a connection.
  if (entry.FailedDownloadCounter() < kNumberOfFailsBeforeWifiOnly) {
    // Try to download the page, whatever the connection.
    reading_list_model_->SetEntryDistilledState(entry.URL(),
                                                ReadingListEntry::PROCESSING);
    url_downloader_->DownloadOfflineURL(entry.URL());

  } else if (entry.FailedDownloadCounter() < kNumberOfFailsBeforeStop) {
    // Try to download the page only if the connection is wifi.
    if (net::NetworkChangeNotifier::GetConnectionType() ==
        net::NetworkChangeNotifier::CONNECTION_WIFI) {
      // The connection is wifi, download the page.
      reading_list_model_->SetEntryDistilledState(entry.URL(),
                                                  ReadingListEntry::PROCESSING);
      url_downloader_->DownloadOfflineURL(entry.URL());

    } else {
      // The connection is not wifi, save it for download when the connection
      // changes to wifi.
      url_to_download_wifi_.push_back(entry.URL());
    }
  }
}

void ReadingListDownloadService::RemoveDownloadedEntry(
    const ReadingListEntry& entry) {
  DCHECK(reading_list_model_->loaded());
  url_downloader_->RemoveOfflineURL(entry.URL());
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
    ScheduleDownloadEntryFromURL(url);

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
      ScheduleDownloadEntryFromURL(url);
    }
  }
  if (type == net::NetworkChangeNotifier::CONNECTION_WIFI) {
    for (auto& url : url_to_download_wifi_) {
      ScheduleDownloadEntryFromURL(url);
    }
  }
}
