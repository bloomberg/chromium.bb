// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/chrome_history_client.h"

#include "base/logging.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/ui/profile_error_dialog.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"

#if defined(OS_ANDROID)
#include "base/files/file_path.h"
#include "chrome/browser/history/android/android_provider_backend.h"
#include "components/history/core/browser/history_backend.h"
#endif

#if defined(OS_ANDROID)
namespace {

const base::FilePath::CharType kAndroidCacheFilename[] =
    FILE_PATH_LITERAL("AndroidCache");

base::FilePath GetAndroidCacheFileName(const base::FilePath& history_dir) {
  return history_dir.Append(kAndroidCacheFilename);
}

}  // namespace
#endif

ChromeHistoryClient::ChromeHistoryClient(
    bookmarks::BookmarkModel* bookmark_model)
    : bookmark_model_(bookmark_model) {
  DCHECK(bookmark_model_);
}

ChromeHistoryClient::~ChromeHistoryClient() {
}

void ChromeHistoryClient::BlockUntilBookmarksLoaded() {
  bookmark_model_->BlockTillLoaded();
}

bool ChromeHistoryClient::IsBookmarked(const GURL& url) {
  return bookmark_model_->IsBookmarked(url);
}

void ChromeHistoryClient::GetBookmarks(
    std::vector<history::URLAndTitle>* bookmarks) {
  std::vector<bookmarks::BookmarkModel::URLAndTitle> bookmarks_url_and_title;
  bookmark_model_->GetBookmarks(&bookmarks_url_and_title);

  bookmarks->reserve(bookmarks->size() + bookmarks_url_and_title.size());
  for (size_t i = 0; i < bookmarks_url_and_title.size(); ++i) {
    history::URLAndTitle value = {
      bookmarks_url_and_title[i].url,
      bookmarks_url_and_title[i].title,
    };
    bookmarks->push_back(value);
  }
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

bool ChromeHistoryClient::ShouldReportDatabaseError() {
  // TODO(shess): For now, don't report on beta or stable so as not to
  // overwhelm the crash server.  Once the big fish are fried,
  // consider reporting at a reduced rate on the bigger channels.
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  return channel != chrome::VersionInfo::CHANNEL_STABLE &&
      channel != chrome::VersionInfo::CHANNEL_BETA;
}

void ChromeHistoryClient::Shutdown() {
  // It's possible that bookmarks haven't loaded and history is waiting for
  // bookmarks to complete loading. In such a situation history can't shutdown
  // (meaning if we invoked HistoryService::Cleanup now, we would deadlock). To
  // break the deadlock we tell BookmarkModel it's about to be deleted so that
  // it can release the signal history is waiting on, allowing history to
  // shutdown (HistoryService::Cleanup to complete). In such a scenario history
  // sees an incorrect view of bookmarks, but it's better than a deadlock.
  bookmark_model_->Shutdown();
}

#if defined(OS_ANDROID)
void ChromeHistoryClient::OnHistoryBackendInitialized(
    history::HistoryBackend* history_backend,
    history::HistoryDatabase* history_database,
    history::ThumbnailDatabase* thumbnail_database,
    const base::FilePath& history_dir) {
  if (thumbnail_database) {
    DCHECK(history_backend);
    history_backend->SetUserData(
        history::AndroidProviderBackend::GetUserDataKey(),
        new history::AndroidProviderBackend(
            GetAndroidCacheFileName(history_dir), history_database,
            thumbnail_database, this, history_backend));
  }
}

void ChromeHistoryClient::OnHistoryBackendDestroyed(
    history::HistoryBackend* history_backend,
    const base::FilePath& history_dir) {
  sql::Connection::Delete(GetAndroidCacheFileName(history_dir));
}
#endif
