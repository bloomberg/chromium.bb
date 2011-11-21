// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/network_action_predictor.h"

#include <math.h>

#include <vector>

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/network_action_predictor_database.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
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

const size_t kMinimumUserTextLength = 2;
const int kMinimumNumberOfHits = 3;

COMPILE_ASSERT(arraysize(kConfidenceCutoff) ==
               NetworkActionPredictor::LAST_PREDICT_ACTION,
               ConfidenceCutoff_count_mismatch);

double OriginalAlgorithm(const history::URLRow& url_row) {
  const double base_score = 1.0;

  // This constant is ln(1/0.65) so we end up decaying to 65% of the base score
  // for each week that passes.
  const double kLnDecayPercent = 0.43078291609245;
  base::TimeDelta time_passed = base::Time::Now() - url_row.last_visit();

  // Clamp to 0.
  const double decay_exponent = std::max(0.0,
      kLnDecayPercent * static_cast<double>(time_passed.InMicroseconds()) /
          base::Time::kMicrosecondsPerWeek);

  const double kMaxDecaySpeedDivisor = 5.0;
  const double kNumUsesPerDecaySpeedDivisorIncrement = 2.0;
  const double decay_divisor = std::min(kMaxDecaySpeedDivisor,
      (url_row.typed_count() + kNumUsesPerDecaySpeedDivisorIncrement - 1) /
          kNumUsesPerDecaySpeedDivisorIncrement);

  return base_score / exp(decay_exponent / decay_divisor);
}

double ConservativeAlgorithm(const history::URLRow& url_row) {
  const double normalized_typed_count =
      std::min(url_row.typed_count() / 5.0, 1.0);
  const double base_score = atan(10 * normalized_typed_count) / atan(10.0);

  // This constant is ln(1/0.65) so we end up decaying to 65% of the base score
  // for each week that passes.
  const double kLnDecayPercent = 0.43078291609245;
  base::TimeDelta time_passed = base::Time::Now() - url_row.last_visit();

  const double decay_exponent = std::max(0.0,
      kLnDecayPercent * static_cast<double>(time_passed.InMicroseconds()) /
          base::Time::kMicrosecondsPerWeek);

  return base_score / exp(decay_exponent);
}

bool GetURLRowForAutocompleteMatch(Profile* profile,
                                   const AutocompleteMatch& match,
                                   history::URLRow* url_row) {
  DCHECK(url_row);

  HistoryService* history_service =
      profile->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history_service)
    return false;

  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  return url_db && (url_db->GetRowForURL(match.destination_url, url_row) != 0);
}

}

const int NetworkActionPredictor::kMaximumDaysToKeepEntry = 14;

NetworkActionPredictor::NetworkActionPredictor(Profile* profile)
    : profile_(profile),
      db_(new NetworkActionPredictorDatabase(profile)),
      initialized_(false) {
  content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
      base::Bind(&NetworkActionPredictorDatabase::Initialize, db_));

  // Request the in-memory database from the history to force it to load so it's
  // available as soon as possible.
  HistoryService* history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (history_service)
    history_service->InMemoryDatabase();

  // Create local caches using the database as loaded. We will garbage collect
  // rows from the caches and the database once the history service is
  // available.
  std::vector<NetworkActionPredictorDatabase::Row>* rows =
      new std::vector<NetworkActionPredictorDatabase::Row>();
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::DB, FROM_HERE,
      base::Bind(&NetworkActionPredictorDatabase::GetAllRows, db_, rows),
      base::Bind(&NetworkActionPredictor::CreateCaches, AsWeakPtr(),
                 base::Owned(rows)));

}

NetworkActionPredictor::~NetworkActionPredictor() {
  db_->OnPredictorDestroyed();
}

void NetworkActionPredictor::RegisterTransitionalMatches(
    const string16& user_text,
    const AutocompleteResult& result) {
  if (prerender::GetOmniboxHeuristicToUse() !=
      prerender::OMNIBOX_HEURISTIC_EXACT_FULL) {
    return;
  }
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

void NetworkActionPredictor::ClearTransitionalMatches() {
  transitional_matches_.clear();
}

// Given a match, return a recommended action.
NetworkActionPredictor::Action NetworkActionPredictor::RecommendAction(
    const string16& user_text,
    const AutocompleteMatch& match) const {
  double confidence = 0.0;

  switch (prerender::GetOmniboxHeuristicToUse()) {
    case prerender::OMNIBOX_HEURISTIC_ORIGINAL: {
      history::URLRow url_row;
      if (GetURLRowForAutocompleteMatch(profile_, match, &url_row))
        confidence = OriginalAlgorithm(url_row);
      break;
    }
    case prerender::OMNIBOX_HEURISTIC_CONSERVATIVE: {
      history::URLRow url_row;
      if (GetURLRowForAutocompleteMatch(profile_, match, &url_row))
        confidence = ConservativeAlgorithm(url_row);
      break;
    }
    case prerender::OMNIBOX_HEURISTIC_EXACT:
    case prerender::OMNIBOX_HEURISTIC_EXACT_FULL:
      confidence = ExactAlgorithm(user_text, match);
      break;
    default:
      NOTREACHED();
      break;
  };

  DCHECK(confidence >= 0.0 && confidence <= 1.0);

  UMA_HISTOGRAM_COUNTS_100("NetworkActionPredictor.Confidence_" +
                           prerender::GetOmniboxHistogramSuffix(),
                           confidence * 100);

  // Map the confidence to an action.
  Action action = ACTION_NONE;
  for (int i = 0; i < LAST_PREDICT_ACTION; ++i) {
    if (confidence >= kConfidenceCutoff[i]) {
      action = static_cast<Action>(i);
      break;
    }
  }

  // Downgrade prerender to preconnect if this is a search match or if omnibox
  // prerendering is disabled.
  if (action == ACTION_PRERENDER &&
      ((match.type == AutocompleteMatch::SEARCH_WHAT_YOU_TYPED ||
       match.type == AutocompleteMatch::SEARCH_SUGGEST ||
       match.type == AutocompleteMatch::SEARCH_OTHER_ENGINE) ||
       !prerender::IsOmniboxEnabled(profile_))) {
    action = ACTION_PRECONNECT;
  }

  return action;
}

// Return true if the suggestion type warrants a TCP/IP preconnection.
// i.e., it is now quite likely that the user will select the related domain.
// static
bool NetworkActionPredictor::IsPreconnectable(const AutocompleteMatch& match) {
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

void NetworkActionPredictor::Observe(
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
        DeleteRowsWithURLs(urls_deleted_details->urls);
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

void NetworkActionPredictor::OnOmniboxOpenedUrl(const AutocompleteLog& log) {
  if (log.text.length() < kMinimumUserTextLength)
    return;

  const GURL& opened_url =
      log.result.match_at(log.selected_index).destination_url;

  const string16 lower_user_text(base::i18n::ToLower(log.text));

  // Add the current match as the only transitional match.
  if (prerender::GetOmniboxHeuristicToUse() !=
      prerender::OMNIBOX_HEURISTIC_EXACT_FULL) {
    DCHECK(transitional_matches_.empty());
    TransitionalMatch dummy_match;
    dummy_match.user_text = lower_user_text;
    dummy_match.urls.push_back(opened_url);
    transitional_matches_.push_back(dummy_match);
  }

  BeginTransaction();
  // Traverse transitional matches for those that have a user_text that is a
  // prefix of |lower_user_text|.
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

      NetworkActionPredictorDatabase::Row row;
      row.user_text = key.user_text;
      row.url = key.url;

      DBCacheMap::iterator it = db_cache_.find(key);
      if (it == db_cache_.end()) {
        row.id = guid::GenerateGUID();
        row.number_of_hits = is_hit ? 1 : 0;
        row.number_of_misses = is_hit ? 0 : 1;

        AddRow(key, row);
      } else {
        DCHECK(db_id_cache_.find(key) != db_id_cache_.end());
        row.id = db_id_cache_.find(key)->second;
        row.number_of_hits = it->second.number_of_hits + (is_hit ? 1 : 0);
        row.number_of_misses = it->second.number_of_misses + (is_hit ? 0 : 1);

        UpdateRow(it, row);
      }
    }
  }
  CommitTransaction();

  ClearTransitionalMatches();
}


void NetworkActionPredictor::DeleteOldIdsFromCaches(
    history::URLDatabase* url_db,
    std::vector<NetworkActionPredictorDatabase::Row::Id>* id_list) {
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

void NetworkActionPredictor::DeleteOldEntries(history::URLDatabase* url_db) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!initialized_);

  std::vector<NetworkActionPredictorDatabase::Row::Id> ids_to_delete;
  DeleteOldIdsFromCaches(url_db, &ids_to_delete);

  content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
      base::Bind(&NetworkActionPredictorDatabase::DeleteRows, db_,
                 ids_to_delete));

  // Register for notifications and set the |initialized_| flag.
  notification_registrar_.Add(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                              content::Source<Profile>(profile_));
  notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                              content::Source<Profile>(profile_));
  initialized_ = true;
}

void NetworkActionPredictor::CreateCaches(
    std::vector<NetworkActionPredictorDatabase::Row>* rows) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!initialized_);
  DCHECK(db_cache_.empty());
  DCHECK(db_id_cache_.empty());

  for (std::vector<NetworkActionPredictorDatabase::Row>::const_iterator it =
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

bool NetworkActionPredictor::TryDeleteOldEntries(HistoryService* service) {
  if (!service)
    return false;

  history::URLDatabase* url_db = service->InMemoryDatabase();
  if (!url_db)
    return false;

  DeleteOldEntries(url_db);
  return true;
}

double NetworkActionPredictor::ExactAlgorithm(
    const string16& user_text,
    const AutocompleteMatch& match) const {
  const DBCacheKey key = { user_text, match.destination_url };
  const DBCacheMap::const_iterator iter = db_cache_.find(key);

  if (iter == db_cache_.end())
    return 0.0;

  const DBCacheValue& value = iter->second;
  if (value.number_of_hits < kMinimumNumberOfHits)
    return 0.0;

  return static_cast<double>(value.number_of_hits) /
      (value.number_of_hits + value.number_of_misses);
}

void NetworkActionPredictor::AddRow(
    const DBCacheKey& key,
    const NetworkActionPredictorDatabase::Row& row) {
  if (!initialized_)
    return;

  DBCacheValue value = { row.number_of_hits, row.number_of_misses };
  db_cache_[key] = value;
  db_id_cache_[key] = row.id;
  content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
      base::Bind(&NetworkActionPredictorDatabase::AddRow, db_, row));
}

void NetworkActionPredictor::UpdateRow(
    DBCacheMap::iterator it,
    const NetworkActionPredictorDatabase::Row& row) {
  if (!initialized_)
    return;

  DCHECK(it != db_cache_.end());
  it->second.number_of_hits = row.number_of_hits;
  it->second.number_of_misses = row.number_of_misses;
  content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
      base::Bind(&NetworkActionPredictorDatabase::UpdateRow, db_, row));
}

void NetworkActionPredictor::DeleteAllRows() {
  if (!initialized_)
    return;

  db_cache_.clear();
  db_id_cache_.clear();
  content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
      base::Bind(&NetworkActionPredictorDatabase::DeleteAllRows, db_));
}

void NetworkActionPredictor::DeleteRowsWithURLs(const std::set<GURL>& urls) {
  if (!initialized_)
    return;

  std::vector<NetworkActionPredictorDatabase::Row::Id> id_list;

  for (DBCacheMap::iterator it = db_cache_.begin(); it != db_cache_.end();) {
    if (urls.find(it->first.url) != urls.end()) {
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
      base::Bind(&NetworkActionPredictorDatabase::DeleteRows, db_, id_list));
}

void NetworkActionPredictor::BeginTransaction() {
  if (!initialized_)
    return;

  content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
      base::Bind(&NetworkActionPredictorDatabase::BeginTransaction, db_));
}

void NetworkActionPredictor::CommitTransaction() {
  if (!initialized_)
    return;

  content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
      base::Bind(&NetworkActionPredictorDatabase::CommitTransaction, db_));
}

NetworkActionPredictor::TransitionalMatch::TransitionalMatch() {
}

NetworkActionPredictor::TransitionalMatch::~TransitionalMatch() {
}
