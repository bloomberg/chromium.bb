// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/downloads_counter.h"

#include "chrome/browser/download/download_history.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/download_manager.h"

DownloadsCounter::DownloadsCounter()
    : pref_name_(prefs::kDeleteDownloadHistory) {
}

DownloadsCounter::~DownloadsCounter() {
}

const std::string& DownloadsCounter::GetPrefName() const {
  return pref_name_;
}

void DownloadsCounter::Count() {
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(GetProfile());
  std::vector<content::DownloadItem*> downloads;
  download_manager->GetAllDownloads(&downloads);
  base::Time begin_time = GetPeriodStart();

  ReportResult(std::count_if(
      downloads.begin(),
      downloads.end(),
      [begin_time](const content::DownloadItem* item) {
        return item->GetStartTime() >= begin_time &&
            DownloadHistory::IsPersisted(item);
      }));
}
