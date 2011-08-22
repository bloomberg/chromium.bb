// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_history.h"

#include "base/logging.h"
#include "chrome/browser/history/download_history_info.h"
#include "chrome/browser/history/history_marshaling.h"
#include "chrome/browser/profiles/profile.h"
#include "content/browser/download/download_item.h"

DownloadHistory::DownloadHistory(Profile* profile)
    : profile_(profile),
      next_fake_db_handle_(DownloadItem::kUninitializedHandle - 1) {
  DCHECK(profile);
}

DownloadHistory::~DownloadHistory() {
  // For any outstanding requests to
  // HistoryService::GetVisibleVisitCountToHost(), since they'll be cancelled
  // and thus not call back to OnGotVisitCountToHost(), we need to delete the
  // associated VisitedBeforeDoneCallbacks.
  for (VisitedBeforeRequestsMap::iterator i(visited_before_requests_.begin());
       i != visited_before_requests_.end(); ++i)
    delete i->second.second;
}

void DownloadHistory::Load(HistoryService::DownloadQueryCallback* callback) {
  DCHECK(callback);
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!hs) {
    delete callback;
    return;
  }
  hs->QueryDownloads(&history_consumer_, callback);

  // This is the initial load, so do a cleanup of corrupt in-progress entries.
  hs->CleanUpInProgressEntries();
}

void DownloadHistory::CheckVisitedReferrerBefore(
    int32 download_id,
    const GURL& referrer_url,
    VisitedBeforeDoneCallback* callback) {
  DCHECK(callback);

  if (referrer_url.is_valid()) {
    HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
    if (hs) {
      HistoryService::Handle handle =
          hs->GetVisibleVisitCountToHost(referrer_url, &history_consumer_,
              NewCallback(this, &DownloadHistory::OnGotVisitCountToHost));
      visited_before_requests_[handle] = std::make_pair(download_id, callback);
      return;
    }
  }
  callback->Run(download_id, false);
  delete callback;
}

void DownloadHistory::AddEntry(
    DownloadItem* download_item,
    HistoryService::DownloadCreateCallback* callback) {
  DCHECK(download_item);
  // Do not store the download in the history database for a few special cases:
  // - incognito mode (that is the point of this mode)
  // - extensions (users don't think of extension installation as 'downloading')
  // - temporary download, like in drag-and-drop
  // - history service is not available (e.g. in tests)
  // We have to make sure that these handles don't collide with normal db
  // handles, so we use a negative value. Eventually, they could overlap, but
  // you'd have to do enough downloading that your ISP would likely stab you in
  // the neck first. YMMV.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (download_item->is_otr() || download_item->is_extension_install() ||
      download_item->is_temporary() || !hs) {
    callback->RunWithParams(
        history::DownloadCreateRequest::TupleType(download_item->id(),
                                                  GetNextFakeDbHandle()));
    delete callback;
    return;
  }

  int32 id = download_item->id();
  DownloadHistoryInfo history_info = download_item->GetHistoryInfo();
  hs->CreateDownload(id, history_info, &history_consumer_, callback);
}

void DownloadHistory::UpdateEntry(DownloadItem* download_item) {
  // Don't store info in the database if the download was initiated while in
  // incognito mode or if it hasn't been initialized in our database table.
  if (download_item->db_handle() <= DownloadItem::kUninitializedHandle)
    return;

  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!hs)
    return;

  hs->UpdateDownload(download_item->received_bytes(),
                     download_item->state(),
                     download_item->db_handle());
}

void DownloadHistory::UpdateDownloadPath(DownloadItem* download_item,
                                         const FilePath& new_path) {
  // No update necessary if the download was initiated while in incognito mode.
  if (download_item->db_handle() <= DownloadItem::kUninitializedHandle)
    return;

  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->UpdateDownloadPath(new_path, download_item->db_handle());
}

void DownloadHistory::RemoveEntry(DownloadItem* download_item) {
  // No update necessary if the download was initiated while in incognito mode.
  if (download_item->db_handle() <= DownloadItem::kUninitializedHandle)
    return;

  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->RemoveDownload(download_item->db_handle());
}

void DownloadHistory::RemoveEntriesBetween(const base::Time remove_begin,
                                           const base::Time remove_end) {
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->RemoveDownloadsBetween(remove_begin, remove_end);
}

int64 DownloadHistory::GetNextFakeDbHandle() {
  return next_fake_db_handle_--;
}

void DownloadHistory::OnGotVisitCountToHost(HistoryService::Handle handle,
                                            bool found_visits,
                                            int count,
                                            base::Time first_visit) {
  VisitedBeforeRequestsMap::iterator request =
      visited_before_requests_.find(handle);
  DCHECK(request != visited_before_requests_.end());
  int32 download_id = request->second.first;
  VisitedBeforeDoneCallback* callback = request->second.second;
  visited_before_requests_.erase(request);
  callback->Run(download_id, found_visits && count &&
      (first_visit.LocalMidnight() < base::Time::Now().LocalMidnight()));
  delete callback;
}
