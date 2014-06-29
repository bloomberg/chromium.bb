// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_history_backend.h"

#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/history/core/browser/url_database.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace history {

InMemoryHistoryBackend::InMemoryHistoryBackend()
    : profile_(NULL) {
}

InMemoryHistoryBackend::~InMemoryHistoryBackend() {}

bool InMemoryHistoryBackend::Init(const base::FilePath& history_filename) {
  db_.reset(new InMemoryDatabase);
  return db_->InitFromDisk(history_filename);
}

void InMemoryHistoryBackend::AttachToHistoryService(Profile* profile) {
  if (!db_) {
    NOTREACHED();
    return;
  }

  profile_ = profile;

  // TODO(evanm): this is currently necessitated by generate_profile, which
  // runs without a browser process. generate_profile should really create
  // a browser process, at which point this check can then be nuked.
  if (!g_browser_process)
    return;

  // Register for the notifications we care about.
  // We only want notifications for the associated profile.
  content::Source<Profile> source(profile_);
  registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URL_VISITED, source);
  registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_MODIFIED, source);
  registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED, source);
  registrar_.Add(
      this, chrome::NOTIFICATION_HISTORY_KEYWORD_SEARCH_TERM_UPDATED, source);
  registrar_.Add(
      this, chrome::NOTIFICATION_HISTORY_KEYWORD_SEARCH_TERM_DELETED, source);
}

void InMemoryHistoryBackend::DeleteAllSearchTermsForKeyword(
    KeywordID keyword_id) {
  // For simplicity, this will not remove the corresponding URLRows, but
  // this is okay, as the main database does not do so either.
  db_->DeleteAllSearchTermsForKeyword(keyword_id);
}

void InMemoryHistoryBackend::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_HISTORY_URL_VISITED:
      OnURLVisitedOrModified(content::Details<URLVisitedDetails>(details)->row);
      break;
    case chrome::NOTIFICATION_HISTORY_KEYWORD_SEARCH_TERM_UPDATED:
      OnKeywordSearchTermUpdated(
          *content::Details<KeywordSearchUpdatedDetails>(details).ptr());
      break;
    case chrome::NOTIFICATION_HISTORY_KEYWORD_SEARCH_TERM_DELETED:
      OnKeywordSearchTermDeleted(
          *content::Details<KeywordSearchDeletedDetails>(details).ptr());
      break;
    case chrome::NOTIFICATION_HISTORY_URLS_MODIFIED: {
      const URLsModifiedDetails* modified_details =
          content::Details<URLsModifiedDetails>(details).ptr();
      URLRows::const_iterator it;
      for (it = modified_details->changed_urls.begin();
           it != modified_details->changed_urls.end(); ++it) {
        OnURLVisitedOrModified(*it);
      }
      break;
    }
    case chrome::NOTIFICATION_HISTORY_URLS_DELETED:
      OnURLsDeleted(*content::Details<URLsDeletedDetails>(details).ptr());
      break;
    default:
      // For simplicity, the unit tests send us all notifications, even when
      // we haven't registered for them, so don't assert here.
      break;
  }
}

void InMemoryHistoryBackend::OnURLVisitedOrModified(const URLRow& url_row) {
  DCHECK(db_);
  DCHECK(url_row.id());
  if (url_row.typed_count() || db_->GetKeywordSearchTermRow(url_row.id(), NULL))
    db_->InsertOrUpdateURLRowByID(url_row);
  else
    db_->DeleteURLRow(url_row.id());
}

void InMemoryHistoryBackend::OnURLsDeleted(const URLsDeletedDetails& details) {
  DCHECK(db_);

  if (details.all_history) {
    // When all history is deleted, the individual URLs won't be listed. Just
    // create a new database to quickly clear everything out.
    db_.reset(new InMemoryDatabase);
    if (!db_->InitFromScratch())
      db_.reset();
    return;
  }

  // Delete all matching URLs in our database.
  for (URLRows::const_iterator row = details.rows.begin();
       row != details.rows.end(); ++row) {
    // This will also delete the corresponding keyword search term.
    // Ignore errors, as we typically only cache a subset of URLRows.
    db_->DeleteURLRow(row->id());
  }
}

void InMemoryHistoryBackend::OnKeywordSearchTermUpdated(
    const KeywordSearchUpdatedDetails& details) {
  DCHECK(details.url_row.id());
  db_->InsertOrUpdateURLRowByID(details.url_row);
  db_->SetKeywordSearchTermsForURL(
      details.url_row.id(), details.keyword_id, details.term);
}

void InMemoryHistoryBackend::OnKeywordSearchTermDeleted(
    const KeywordSearchDeletedDetails& details) {
  // For simplicity, this will not remove the corresponding URLRow, but this is
  // okay, as the main database does not do so either.
  db_->DeleteKeywordSearchTermForURL(details.url_row_id);
}

}  // namespace history
