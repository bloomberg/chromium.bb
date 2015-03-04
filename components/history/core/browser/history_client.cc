// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_client.h"

namespace history {

HistoryClient::HistoryClient() {
}

void HistoryClient::BlockUntilBookmarksLoaded() {
}

bool HistoryClient::IsBookmarked(const GURL& url) {
  return false;
}

void HistoryClient::GetBookmarks(std::vector<URLAndTitle>* bookmarks) {
}

bool HistoryClient::CanAddURL(const GURL& url) {
  return true;
}

void HistoryClient::NotifyProfileError(sql::InitStatus init_status) {
}

bool HistoryClient::ShouldReportDatabaseError() {
  return false;
}

void HistoryClient::OnHistoryBackendInitialized(
    HistoryBackend* history_backend,
    HistoryDatabase* history_database,
    ThumbnailDatabase* thumbnail_database,
    const base::FilePath& history_dir) {
}

void HistoryClient::OnHistoryBackendDestroyed(
    HistoryBackend* history_backend,
    const base::FilePath& history_dir) {
}

}  // namespace history
