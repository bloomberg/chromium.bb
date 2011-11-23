// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/in_memory_history_backend.h"

#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

namespace history {

InMemoryHistoryBackend::InMemoryHistoryBackend()
    : profile_(NULL) {}

InMemoryHistoryBackend::~InMemoryHistoryBackend() {}

bool InMemoryHistoryBackend::Init(const FilePath& history_filename,
                                  const FilePath& history_dir,
                                  URLDatabase* db,
                                  const std::string& languages) {
  db_.reset(new InMemoryDatabase);
  return db_->InitFromDisk(history_filename);
}

void InMemoryHistoryBackend::AttachToHistoryService(Profile* profile) {
  if (!db_.get()) {
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
  registrar_.Add(this, chrome::NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED,
                 source);
  registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED, source);
  registrar_.Add(this,
                 chrome::NOTIFICATION_HISTORY_KEYWORD_SEARCH_TERM_UPDATED,
                 source);
  registrar_.Add(this, chrome::NOTIFICATION_TEMPLATE_URL_REMOVED, source);
}

void InMemoryHistoryBackend::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_HISTORY_URL_VISITED: {
      content::Details<history::URLVisitedDetails> visited_details(details);
      content::PageTransition primary_type =
          content::PageTransitionStripQualifier(visited_details->transition);
      if (visited_details->row.typed_count() > 0 ||
          primary_type == content::PAGE_TRANSITION_KEYWORD ||
          HasKeyword(visited_details->row.url())) {
        URLsModifiedDetails modified_details;
        modified_details.changed_urls.push_back(visited_details->row);
        OnTypedURLsModified(modified_details);
      }
      break;
    }
    case chrome::NOTIFICATION_HISTORY_KEYWORD_SEARCH_TERM_UPDATED:
      OnKeywordSearchTermUpdated(
          *content::Details<history::KeywordSearchTermDetails>(details).ptr());
      break;
    case chrome::NOTIFICATION_HISTORY_TYPED_URLS_MODIFIED:
      OnTypedURLsModified(
          *content::Details<history::URLsModifiedDetails>(details).ptr());
      break;
    case chrome::NOTIFICATION_HISTORY_URLS_DELETED:
      OnURLsDeleted(
          *content::Details<history::URLsDeletedDetails>(details).ptr());
      break;
    case chrome::NOTIFICATION_TEMPLATE_URL_REMOVED:
      db_->DeleteAllSearchTermsForKeyword(
          *(content::Details<TemplateURLID>(details).ptr()));
      break;
    default:
      // For simplicity, the unit tests send us all notifications, even when
      // we haven't registered for them, so don't assert here.
      break;
  }
}

void InMemoryHistoryBackend::OnTypedURLsModified(
    const URLsModifiedDetails& details) {
  DCHECK(db_.get());

  // Add or update the URLs.
  //
  // TODO(brettw) currently the rows in the in-memory database don't match the
  // IDs in the main database. This sucks. Instead of Add and Remove, we should
  // have Sync(), which would take the ID if it's given and add it.
  std::vector<history::URLRow>::const_iterator i;
  for (i = details.changed_urls.begin();
       i != details.changed_urls.end(); i++) {
    URLID id = db_->GetRowForURL(i->url(), NULL);
    if (id)
      db_->UpdateURLRow(id, *i);
    else
      id = db_->AddURL(*i);
  }
}

void InMemoryHistoryBackend::OnURLsDeleted(const URLsDeletedDetails& details) {
  DCHECK(db_.get());
  if (details.all_history) {
    // When all history is deleted, the individual URLs won't be listed. Just
    // create a new database to quickly clear everything out.
    db_.reset(new InMemoryDatabase);
    if (!db_->InitFromScratch())
      db_.reset();
    return;
  }

  // Delete all matching URLs in our database.
  for (std::vector<URLRow>::const_iterator row = details.rows.begin();
       row != details.rows.end(); ++row) {
    // We typically won't have most of them since we only have a subset of
    // history, so ignore errors.
    db_->DeleteURLRow(row->id());
  }
}

void InMemoryHistoryBackend::OnKeywordSearchTermUpdated(
    const KeywordSearchTermDetails& details) {
  // The url won't exist for new search terms (as the user hasn't typed it), so
  // we force it to be added. If we end up adding a URL it won't be
  // autocompleted as the typed count is 0.
  URLRow url_row;
  URLID url_id;
  if (!db_->GetRowForURL(details.url, &url_row)) {
    // Because this row won't have a typed count the title and other stuff
    // doesn't matter. If the user ends up typing the url we'll update the title
    // in OnTypedURLsModified.
    URLRow new_row(details.url);
    new_row.set_last_visit(base::Time::Now());
    url_id = db_->AddURL(new_row);
    if (!url_id)
      return;  // Error adding.
  } else {
    url_id = url_row.id();
  }

  db_->SetKeywordSearchTermsForURL(url_id, details.keyword_id, details.term);
}

bool InMemoryHistoryBackend::HasKeyword(const GURL& url) {
  URLID id = db_->GetRowForURL(url, NULL);
  if (!id)
    return false;

  return db_->GetKeywordSearchTermRow(id, NULL);
}

}  // namespace history
