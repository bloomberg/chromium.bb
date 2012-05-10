// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/autocomplete_action_predictor.h"

#include <math.h>

#include <vector>

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/guid.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"

namespace {

const float kConfidenceCutoff[] = {
  0.8f,
  0.5f
};

COMPILE_ASSERT(arraysize(kConfidenceCutoff) ==
               predictors::AutocompleteActionPredictor::LAST_PREDICT_ACTION,
               ConfidenceCutoff_count_mismatch);

const size_t kMinimumUserTextLength = 1;
const int kMinimumNumberOfHits = 3;

enum DatabaseAction {
  DATABASE_ACTION_ADD,
  DATABASE_ACTION_UPDATE,
  DATABASE_ACTION_DELETE_SOME,
  DATABASE_ACTION_DELETE_ALL,
  DATABASE_ACTION_COUNT
};

bool IsAutocompleteMatchSearchType(const AutocompleteMatch& match) {
  switch (match.type) {
    // Matches using the user's default search engine.
    case AutocompleteMatch::SEARCH_WHAT_YOU_TYPED:
    case AutocompleteMatch::SEARCH_HISTORY:
    case AutocompleteMatch::SEARCH_SUGGEST:
    // A match that uses a non-default search engine (e.g. for tab-to-search).
    case AutocompleteMatch::SEARCH_OTHER_ENGINE:
      return true;

    default:
      return false;
  }
}

}  // namespace

namespace predictors {

const int AutocompleteActionPredictor::kMaximumDaysToKeepEntry = 14;

double AutocompleteActionPredictor::hit_weight_ = 1.0;

AutocompleteActionPredictor::AutocompleteActionPredictor(Profile* profile)
    : profile_(profile),
      table_(PredictorDatabaseFactory::GetForProfile(
          profile)->autocomplete_table()),
      initialized_(false) {
  // Request the in-memory database from the history to force it to load so it's
  // available as soon as possible.
  HistoryService* history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (history_service)
    history_service->InMemoryDatabase();

  // Create local caches using the database as loaded. We will garbage collect
  // rows from the caches and the database once the history service is
  // available.
  std::vector<AutocompleteActionPredictorTable::Row>* rows =
      new std::vector<AutocompleteActionPredictorTable::Row>();
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::DB, FROM_HERE,
      base::Bind(&AutocompleteActionPredictorTable::GetAllRows,
                 table_,
                 rows),
      base::Bind(&AutocompleteActionPredictor::CreateCaches, AsWeakPtr(),
                 base::Owned(rows)));
}

AutocompleteActionPredictor::~AutocompleteActionPredictor() {
}

void AutocompleteActionPredictor::RegisterTransitionalMatches(
    const string16& user_text,
    const AutocompleteResult& result) {
  if (user_text.length() < kMinimumUserTextLength)
    return;
  const string16 lower_user_text(base::i18n::ToLower(user_text));

  // Merge this in to an existing match if we already saw |user_text|
  std::vector<TransitionalMatch>::iterator match_it =
      std::find(transitional_matches_.begin(), transitional_matches_.end(),
                lower_user_text);

  if (match_it == transitional_matches_.end()) {
    TransitionalMatch transitional_match;
    transitional_match.user_text = lower_user_text;
    match_it = transitional_matches_.insert(transitional_matches_.end(),
                                            transitional_match);
  }

  for (AutocompleteResult::const_iterator it = result.begin();
       it != result.end(); ++it) {
    if (std::find(match_it->urls.begin(), match_it->urls.end(),
                  it->destination_url) == match_it->urls.end()) {
      match_it->urls.push_back(it->destination_url);
    }
  }
}

void AutocompleteActionPredictor::ClearTransitionalMatches() {
  transitional_matches_.clear();
}

// Given a match, return a recommended action.
AutocompleteActionPredictor::Action
    AutocompleteActionPredictor::RecommendAction(
        const string16& user_text,
        const AutocompleteMatch& match) const {
  bool is_in_db = false;
  const double confidence = CalculateConfidence(user_text, match, &is_in_db);
  DCHECK(confidence >= 0.0 && confidence <= 1.0);

  UMA_HISTOGRAM_BOOLEAN("AutocompleteActionPredictor.MatchIsInDb", is_in_db);

  if (is_in_db) {
    // Multiple enties with the same URL are fine as the confidence may be
    // different.
    tracked_urls_.push_back(std::make_pair(match.destination_url, confidence));
    UMA_HISTOGRAM_COUNTS_100("AutocompleteActionPredictor.Confidence",
                             confidence * 100);
  }

  // Map the confidence to an action.
  Action action = ACTION_NONE;
  for (int i = 0; i < LAST_PREDICT_ACTION; ++i) {
    if (confidence >= kConfidenceCutoff[i]) {
      action = static_cast<Action>(i);
      break;
    }
  }

  // Downgrade prerender to preconnect if this is a search match or if omnibox
  // prerendering is disabled. There are cases when Instant will not handle a
  // search suggestion and in those cases it would be good to prerender the
  // search results, however search engines have not been set up to correctly
  // handle being prerendered and until they are we should avoid it.
  // http://crbug.com/117495
  if (action == ACTION_PRERENDER &&
      (IsAutocompleteMatchSearchType(match) ||
       !prerender::IsOmniboxEnabled(profile_))) {
    action = ACTION_PRECONNECT;
  }

  return action;
}

// Return true if the suggestion type warrants a TCP/IP preconnection.
// i.e., it is now quite likely that the user will select the related domain.
// static
bool AutocompleteActionPredictor::IsPreconnectable(
    const AutocompleteMatch& match) {
  return IsAutocompleteMatchSearchType(match);
}

void AutocompleteActionPredictor::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_HISTORY_URLS_DELETED: {
      DCHECK(initialized_);
      const content::Details<const history::URLsDeletedDetails>
          urls_deleted_details =
              content::Details<const history::URLsDeletedDetails>(details);
      if (urls_deleted_details->all_history)
        DeleteAllRows();
      else
        DeleteRowsWithURLs(urls_deleted_details->rows);
      break;
    }

    // This notification does not catch all instances of the user navigating
    // from the Omnibox, but it does catch the cases where the dropdown is open
    // and those are the events we're most interested in.
    case chrome::NOTIFICATION_OMNIBOX_OPENED_URL: {
      DCHECK(initialized_);

      // TODO(dominich): This doesn't need to be synchronous. Investigate
      // posting it as a task to be run later.
      OnOmniboxOpenedUrl(*content::Details<AutocompleteLog>(details).ptr());
      break;
    }

    case chrome::NOTIFICATION_HISTORY_LOADED: {
      DCHECK(!initialized_);
      TryDeleteOldEntries(content::Details<HistoryService>(details).ptr());

      notification_registrar_.Remove(this,
                                     chrome::NOTIFICATION_HISTORY_LOADED,
                                     content::Source<Profile>(profile_));
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification observed.";
      break;
  }
}

void AutocompleteActionPredictor::OnOmniboxOpenedUrl(
    const AutocompleteLog& log) {
  if (log.text.length() < kMinimumUserTextLength)
    return;

  const AutocompleteMatch& match = log.result.match_at(log.selected_index);

  UMA_HISTOGRAM_BOOLEAN(
      StringPrintf("Prerender.OmniboxNavigationsCouldPrerender_%.1f%s",
                   get_hit_weight(),
                   prerender::PrerenderManager::GetModeString()).c_str(),
      prerender::IsOmniboxEnabled(profile_));

  const GURL& opened_url = match.destination_url;

  // If the Omnibox triggered a prerender but the URL doesn't match the one the
  // user is navigating to, cancel the prerender.
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile_);
  // |prerender_manager| can be NULL in incognito mode or if prerendering is
  // otherwise disabled.
  if (prerender_manager && !prerender_manager->IsPrerendering(opened_url))
    prerender_manager->CancelOmniboxPrerenders();

  const string16 lower_user_text(base::i18n::ToLower(log.text));

  // Traverse transitional matches for those that have a user_text that is a
  // prefix of |lower_user_text|.
  std::vector<AutocompleteActionPredictorTable::Row> rows_to_add;
  std::vector<AutocompleteActionPredictorTable::Row> rows_to_update;

  for (std::vector<TransitionalMatch>::const_iterator it =
       transitional_matches_.begin(); it != transitional_matches_.end();
       ++it) {
    if (!StartsWith(lower_user_text, it->user_text, true))
      continue;

    // Add entries to the database for those matches.
    for (std::vector<GURL>::const_iterator url_it = it->urls.begin();
         url_it != it->urls.end(); ++url_it) {
      DCHECK(it->user_text.length() >= kMinimumUserTextLength);
      const DBCacheKey key = { it->user_text, *url_it };
      const bool is_hit = (*url_it == opened_url);

      AutocompleteActionPredictorTable::Row row;
      row.user_text = key.user_text;
      row.url = key.url;

      DBCacheMap::iterator it = db_cache_.find(key);
      if (it == db_cache_.end()) {
        row.id = guid::GenerateGUID();
        row.number_of_hits = is_hit ? 1 : 0;
        row.number_of_misses = is_hit ? 0 : 1;

        rows_to_add.push_back(row);
      } else {
        DCHECK(db_id_cache_.find(key) != db_id_cache_.end());
        row.id = db_id_cache_.find(key)->second;
        row.number_of_hits = it->second.number_of_hits + (is_hit ? 1 : 0);
        row.number_of_misses = it->second.number_of_misses + (is_hit ? 0 : 1);

        rows_to_update.push_back(row);
      }
    }
  }
  if (rows_to_add.size() > 0 || rows_to_update.size() > 0)
    AddAndUpdateRows(rows_to_add, rows_to_update);

  ClearTransitionalMatches();

  // Check against tracked urls and log accuracy for the confidence we
  // predicted.
  for (std::vector<std::pair<GURL, double> >::const_iterator it =
       tracked_urls_.begin(); it != tracked_urls_.end();
       ++it) {
    if (opened_url == it->first) {
      UMA_HISTOGRAM_COUNTS_100("AutocompleteActionPredictor.AccurateCount",
                               it->second * 100);
    }
  }
  tracked_urls_.clear();
}

void AutocompleteActionPredictor::DeleteOldIdsFromCaches(
    history::URLDatabase* url_db,
    std::vector<AutocompleteActionPredictorTable::Row::Id>* id_list) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(url_db);
  DCHECK(id_list);
  id_list->clear();
  for (DBCacheMap::iterator it = db_cache_.begin(); it != db_cache_.end();) {
    history::URLRow url_row;

    if ((url_db->GetRowForURL(it->first.url, &url_row) == 0) ||
        ((base::Time::Now() - url_row.last_visit()).InDays() >
         kMaximumDaysToKeepEntry)) {
      const DBIdCacheMap::iterator id_it = db_id_cache_.find(it->first);
      DCHECK(id_it != db_id_cache_.end());
      id_list->push_back(id_it->second);
      db_id_cache_.erase(id_it);
      db_cache_.erase(it++);
    } else {
      ++it;
    }
  }
}

void AutocompleteActionPredictor::DeleteOldEntries(
    history::URLDatabase* url_db) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!initialized_);

  std::vector<AutocompleteActionPredictorTable::Row::Id> ids_to_delete;
  DeleteOldIdsFromCaches(url_db, &ids_to_delete);

  content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
      base::Bind(&AutocompleteActionPredictorTable::DeleteRows,
                 table_,
                 ids_to_delete));

  // Register for notifications and set the |initialized_| flag.
  notification_registrar_.Add(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                              content::Source<Profile>(profile_));
  notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                              content::Source<Profile>(profile_));
  initialized_ = true;
}

void AutocompleteActionPredictor::CreateCaches(
    std::vector<AutocompleteActionPredictorTable::Row>* rows) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!initialized_);
  DCHECK(db_cache_.empty());
  DCHECK(db_id_cache_.empty());

  for (std::vector<AutocompleteActionPredictorTable::Row>::const_iterator it =
       rows->begin(); it != rows->end(); ++it) {
    const DBCacheKey key = { it->user_text, it->url };
    const DBCacheValue value = { it->number_of_hits, it->number_of_misses };
    db_cache_[key] = value;
    db_id_cache_[key] = it->id;
  }

  // If the history service is ready, delete any old or invalid entries.
  HistoryService* history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!TryDeleteOldEntries(history_service)) {
    // Wait for the notification that the history service is ready and the URL
    // DB is loaded.
    notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_LOADED,
                                content::Source<Profile>(profile_));
  }
}

bool AutocompleteActionPredictor::TryDeleteOldEntries(HistoryService* service) {
  if (!service)
    return false;

  history::URLDatabase* url_db = service->InMemoryDatabase();
  if (!url_db)
    return false;

  DeleteOldEntries(url_db);
  return true;
}

double AutocompleteActionPredictor::CalculateConfidence(
    const string16& user_text,
    const AutocompleteMatch& match,
    bool* is_in_db) const {
  const DBCacheKey key = { user_text, match.destination_url };

  *is_in_db = false;
  if (user_text.length() < kMinimumUserTextLength)
    return 0.0;

  const DBCacheMap::const_iterator iter = db_cache_.find(key);
  if (iter == db_cache_.end())
    return 0.0;

  *is_in_db = true;
  return CalculateConfidenceForDbEntry(iter);
}

double AutocompleteActionPredictor::CalculateConfidenceForDbEntry(
    DBCacheMap::const_iterator iter) const {
  const DBCacheValue& value = iter->second;
  if (value.number_of_hits < kMinimumNumberOfHits)
    return 0.0;

  const double number_of_hits = value.number_of_hits * hit_weight_;
  return number_of_hits / (number_of_hits + value.number_of_misses);
}

void AutocompleteActionPredictor::AddAndUpdateRows(
    const AutocompleteActionPredictorTable::Rows& rows_to_add,
    const AutocompleteActionPredictorTable::Rows& rows_to_update) {
  if (!initialized_)
    return;

  for (AutocompleteActionPredictorTable::Rows::const_iterator it =
       rows_to_add.begin(); it != rows_to_add.end(); ++it) {
    const DBCacheKey key = { it->user_text, it->url };
    DBCacheValue value = { it->number_of_hits, it->number_of_misses };

    DCHECK(db_cache_.find(key) == db_cache_.end());

    db_cache_[key] = value;
    db_id_cache_[key] = it->id;
    UMA_HISTOGRAM_ENUMERATION("AutocompleteActionPredictor.DatabaseAction",
                              DATABASE_ACTION_ADD, DATABASE_ACTION_COUNT);
  }
  for (AutocompleteActionPredictorTable::Rows::const_iterator it =
       rows_to_update.begin(); it != rows_to_update.end(); ++it) {
    const DBCacheKey key = { it->user_text, it->url };

    DBCacheMap::iterator db_it = db_cache_.find(key);
    DCHECK(db_it != db_cache_.end());
    DCHECK(db_id_cache_.find(key) != db_id_cache_.end());

    db_it->second.number_of_hits = it->number_of_hits;
    db_it->second.number_of_misses = it->number_of_misses;
    UMA_HISTOGRAM_ENUMERATION("AutocompleteActionPredictor.DatabaseAction",
                              DATABASE_ACTION_UPDATE, DATABASE_ACTION_COUNT);
  }

  content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
      base::Bind(&AutocompleteActionPredictorTable::AddAndUpdateRows,
                 table_,
                 rows_to_add,
                 rows_to_update));
}

void AutocompleteActionPredictor::DeleteAllRows() {
  if (!initialized_)
    return;

  db_cache_.clear();
  db_id_cache_.clear();
  content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
      base::Bind(&AutocompleteActionPredictorTable::DeleteAllRows,
                 table_));
  UMA_HISTOGRAM_ENUMERATION("AutocompleteActionPredictor.DatabaseAction",
                            DATABASE_ACTION_DELETE_ALL, DATABASE_ACTION_COUNT);
}

void AutocompleteActionPredictor::DeleteRowsWithURLs(
    const history::URLRows& rows) {
  if (!initialized_)
    return;

  std::vector<AutocompleteActionPredictorTable::Row::Id> id_list;

  for (DBCacheMap::iterator it = db_cache_.begin(); it != db_cache_.end();) {
    if (std::find_if(rows.begin(), rows.end(),
        history::URLRow::URLRowHasURL(it->first.url)) != rows.end()) {
      const DBIdCacheMap::iterator id_it = db_id_cache_.find(it->first);
      DCHECK(id_it != db_id_cache_.end());
      id_list.push_back(id_it->second);
      db_id_cache_.erase(id_it);
      db_cache_.erase(it++);
    } else {
      ++it;
    }
  }

  content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
      base::Bind(&AutocompleteActionPredictorTable::DeleteRows, table_,
                 id_list));
  UMA_HISTOGRAM_ENUMERATION("AutocompleteActionPredictor.DatabaseAction",
                            DATABASE_ACTION_DELETE_SOME, DATABASE_ACTION_COUNT);
}

AutocompleteActionPredictor::TransitionalMatch::TransitionalMatch() {
}

AutocompleteActionPredictor::TransitionalMatch::~TransitionalMatch() {
}

}  // namespace predictors
