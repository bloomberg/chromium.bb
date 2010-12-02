// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_history.h"

#include "base/logging.h"
#include "chrome/browser/history/download_create_info.h"
#include "chrome/browser/history/history_marshaling.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/profiles/profile.h"

// Our download table ID starts at 1, so we use 0 to represent a download that
// has started, but has not yet had its data persisted in the table. We use fake
// database handles in incognito mode starting at -1 and progressively getting
// more negative.
// static
const int DownloadHistory::kUninitializedHandle = 0;

DownloadHistory::DownloadHistory(Profile* profile)
    : profile_(profile),
      next_fake_db_handle_(kUninitializedHandle - 1) {
  DCHECK(profile);
}

DownloadHistory::~DownloadHistory() {
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

void DownloadHistory::AddEntry(
    const DownloadCreateInfo& info,
    DownloadItem* download_item,
    HistoryService::DownloadCreateCallback* callback) {
  // Do not store the download in the history database for a few special cases:
  // - incognito mode (that is the point of this mode)
  // - extensions (users don't think of extension installation as 'downloading')
  // - temporary download, like in drag-and-drop
  // - history service is not available (e.g. in tests)
  // We have to make sure that these handles don't collide with normal db
  // handles, so we use a negative value. Eventually, they could overlap, but
  // you'd have to do enough downloading that your ISP would likely stab you in
  // the neck first. YMMV.
  // TODO(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (download_item->is_otr() || download_item->is_extension_install() ||
      download_item->is_temporary() || !hs) {
    callback->RunWithParams(
        history::DownloadCreateRequest::TupleType(info, GetNextFakeDbHandle()));
    delete callback;
    return;
  }

  hs->CreateDownload(info, &history_consumer_, callback);
}

void DownloadHistory::UpdateEntry(DownloadItem* download_item) {
  // Don't store info in the database if the download was initiated while in
  // incognito mode or if it hasn't been initialized in our database table.
  if (download_item->db_handle() <= kUninitializedHandle)
    return;

  // TODO(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
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
  if (download_item->db_handle() <= kUninitializedHandle)
    return;

  // TODO(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->UpdateDownloadPath(new_path, download_item->db_handle());
}

void DownloadHistory::RemoveEntry(DownloadItem* download_item) {
  // No update necessary if the download was initiated while in incognito mode.
  if (download_item->db_handle() <= kUninitializedHandle)
    return;

  // TODO(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->RemoveDownload(download_item->db_handle());
}

void DownloadHistory::RemoveEntriesBetween(const base::Time remove_begin,
                                           const base::Time remove_end) {
  // TODO(paulg) see bug 958058. EXPLICIT_ACCESS below is wrong.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (hs)
    hs->RemoveDownloadsBetween(remove_begin, remove_end);
}

int64 DownloadHistory::GetNextFakeDbHandle() {
  return next_fake_db_handle_--;
}
