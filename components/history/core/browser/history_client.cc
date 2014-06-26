// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_client.h"

namespace history {

void HistoryClient::BlockUntilBookmarksLoaded() {
}

bool HistoryClient::IsBookmarked(const GURL& url) {
  return false;
}

void HistoryClient::GetBookmarks(std::vector<URLAndTitle>* bookmarks) {
}

void HistoryClient::NotifyProfileError(sql::InitStatus init_status) {
}

bool HistoryClient::ShouldReportDatabaseError() {
  return false;
}

HistoryClient::HistoryClient() {
}

}
