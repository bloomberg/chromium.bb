// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/chrome_history_client.h"

#include "chrome/browser/history/chrome_history_backend_client.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"

ChromeHistoryClient::ChromeHistoryClient(
    bookmarks::BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model) {
}

ChromeHistoryClient::~ChromeHistoryClient() {
}

void ChromeHistoryClient::Shutdown() {
  // It's possible that bookmarks haven't loaded and history is waiting for
  // bookmarks to complete loading. In such a situation history can't shutdown
  // (meaning if we invoked HistoryService::Cleanup now, we would deadlock). To
  // break the deadlock we tell BookmarkModel it's about to be deleted so that
  // it can release the signal history is waiting on, allowing history to
  // shutdown (HistoryService::Cleanup to complete). In such a scenario history
  // sees an incorrect view of bookmarks, but it's better than a deadlock.
  if (bookmark_model_)
    bookmark_model_->Shutdown();
}

bool ChromeHistoryClient::CanAddURL(const GURL& url) {
  return CanAddURLToHistory(url);
}

void ChromeHistoryClient::NotifyProfileError(sql::InitStatus init_status) {
  ShowProfileErrorDialog(
      PROFILE_ERROR_HISTORY,
      (init_status == sql::INIT_FAILURE) ?
      IDS_COULDNT_OPEN_PROFILE_ERROR : IDS_PROFILE_TOO_NEW_ERROR);
}

scoped_ptr<history::HistoryBackendClient>
ChromeHistoryClient::CreateBackendClient() {
  return make_scoped_ptr(new ChromeHistoryBackendClient(bookmark_model_));
}
