// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/autocomplete_action_predictor.h"

#include <math.h>

#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/omnibox/omnibox_log.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_popup_model.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/omnibox/autocomplete_match.h"
#include "components/omnibox/autocomplete_result.h"
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

}  // namespace

namespace predictors {

const int AutocompleteActionPredictor::kMaximumDaysToKeepEntry = 14;

AutocompleteActionPredictor::AutocompleteActionPredictor(Profile* profile)
    : profile_(profile),
      main_profile_predictor_(NULL),
      incognito_predictor_(NULL),
      initialized_(false) {
  if (profile_->IsOffTheRecord()) {
    main_profile_predictor_ = AutocompleteActionPredictorFactory::GetForProfile(
        profile_->GetOriginalProfile());
    DCHECK(main_profile_predictor_);
    main_profile_predictor_->incognito_predictor_ = this;
    if (main_profile_predictor_->initialized_)
      CopyFromMainProfile();
  } else {
    // Request the in-memory database from the history to force it to load so
    // it's available as soon as possible.
    HistoryService* history_service = HistoryServiceFactory::GetForProfile(
        profile_, Profile::EXPLICIT_ACCESS);
    if (history_service)
      history_service->InMemoryDatabase();

    table_ =
        PredictorDatabaseFactory::GetForProfile(profile_)->autocomplete_table();

    // Observe all main frame loads so we can wait for the first to complete
    // before accessing DB and IO threads to build the local cache.
    notification_registrar_.Add(this,
                                content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                                content::NotificationService::AllSources());
  }
}

AutocompleteActionPredictor::~AutocompleteActionPredictor() {
  if (main_profile_predictor_)
    main_profile_predictor_->incognito_predictor_ = NULL;
  else if (incognito_predictor_)
    incognito_predictor_->main_profile_predictor_ = NULL;
  if (prerender_handle_.get())
    prerender_handle_->OnCancel();
}

void AutocompleteActionPredictor::RegisterTransitionalMatches(
    const base::string16& user_text,
    const AutocompleteResult& result) {
  if (user_text.length() < kMinimumUserTextLength)
    return;
  const base::string16 lower_user_text(base::i18n::ToLower(user_text));

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

  for (AutocompleteResult::const_iterator i(result.begin()); i != result.end();
       ++i) {
    if (std::find(match_it->urls.begin(), match_it->urls.end(),
                  i->destination_url) == match_it->urls.end()) {
      match_it->urls.push_back(i->destination_url);
    }
  }
}

void AutocompleteActionPredictor::ClearTransitionalMatches() {
  transitional_matches_.clear();
}

void AutocompleteActionPredictor::CancelPrerender() {
  // If the prerender has already been abandoned, leave it to its own timeout;
  // this normally gets called immediately after OnOmniboxOpenedUrl.
  if (prerender_handle_ && !prerender_handle_->IsAbandoned()) {
    prerender_handle_->OnCancel();
    prerender_handle_.reset();
  }
}

void AutocompleteActionPredictor::StartPrerendering(
    const GURL& url,
    const content::SessionStorageNamespaceMap& session_storage_namespace_map,
    const gfx::Size& size) {
  // Only cancel the old prerender after starting the new one, so if the URLs
  // are the same, the underlying prerender will be reused.
  scoped_ptr<prerender::PrerenderHandle> old_prerender_handle(
      prerender_handle_.release());
  if (prerender::PrerenderManager* prerender_manager =
          prerender::PrerenderManagerFactory::GetForProfile(profile_)) {
    content::SessionStorageNamespace* session_storage_namespace = NULL;
    content::SessionStorageNamespaceMap::const_iterator it =
        session_storage_namespace_map.find(std::string());
    if (it != session_storage_namespace_map.end())
      session_storage_namespace = it->second.get();
    prerender_handle_.reset(prerender_manager->AddPrerenderFromOmnibox(
        url, session_storage_namespace, size));
  }
  if (old_prerender_handle)
    old_prerender_handle->OnCancel();
}

// Given a match, return a recommended action.
AutocompleteActionPredictor::Action
    AutocompleteActionPredictor::RecommendAction(
        const base::string16& user_text,
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
      (AutocompleteMatch::IsSearchType(match.type) ||
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
  return AutocompleteMatch::IsSearchType(match.type);
}

bool AutocompleteActionPredictor::IsPrerenderAbandonedForTesting() {
  return prerender_handle_ && prerender_handle_->IsAbandoned();
}

void AutocompleteActionPredictor::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME:
      CreateLocalCachesFromDatabase();
      notification_registrar_.Remove(
          this,
          content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
          content::NotificationService::AllSources());
      break;

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

    case chrome::NOTIFICATION_OMNIBOX_OPENED_URL: {
      DCHECK(initialized_);

      // TODO(dominich): This doesn't need to be synchronous. Investigate
      // posting it as a task to be run later.
      OnOmniboxOpenedUrl(*content::Details<OmniboxLog>(details).ptr());
      break;
    }

    case chrome::NOTIFICATION_HISTORY_LOADED: {
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

void AutocompleteActionPredictor::CreateLocalCachesFromDatabase() {
  // Create local caches using the database as loaded. We will garbage collect
  // rows from the caches and the database once the history service is
  // available.
  std::vector<AutocompleteActionPredictorTable::Row>* rows =
      new std::vector<AutocompleteActionPredictorTable::Row>();
  content::BrowserThread::PostTaskAndReply(content::BrowserThread::DB,
      FROM_HERE,
      base::Bind(&AutocompleteActionPredictorTable::GetAllRows, table_, rows),
      base::Bind(&AutocompleteActionPredictor::CreateCaches, AsWeakPtr(),
                 base::Owned(rows)));
}

void AutocompleteActionPredictor::DeleteAllRows() {
  if (!initialized_)
    return;

  db_cache_.clear();
  db_id_cache_.clear();

  if (table_.get()) {
    content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
        base::Bind(&AutocompleteActionPredictorTable::DeleteAllRows,
                   table_));
  }

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

  if (table_.get()) {
    content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
        base::Bind(&AutocompleteActionPredictorTable::DeleteRows, table_,
                   id_list));
  }

  UMA_HISTOGRAM_ENUMERATION("AutocompleteActionPredictor.DatabaseAction",
                            DATABASE_ACTION_DELETE_SOME, DATABASE_ACTION_COUNT);
}

void AutocompleteActionPredictor::OnOmniboxOpenedUrl(const OmniboxLog& log) {
  if (log.text.length() < kMinimumUserTextLength)
    return;

  // Do not attempt to learn from omnibox interactions where the omnibox
  // dropdown is closed.  In these cases the user text (|log.text|) that we
  // learn from is either empty or effectively identical to the destination
  // string.  In either case, it can't teach us much.  Also do not attempt
  // to learn from paste-and-go actions even if the popup is open because
  // the paste-and-go destination has no relation to whatever text the user
  // may have typed.
  if (!log.is_popup_open || log.is_paste_and_go)
    return;

  // Abandon the current prerender. If it is to be used, it will be used very
  // soon, so use the lower timeout.
  if (prerender_handle_) {
    prerender_handle_->OnNavigateAway();
    // Don't release |prerender_handle_| so it is canceled if it survives to the
    // next StartPrerendering call.
  }

  UMA_HISTOGRAM_BOOLEAN(
      base::StringPrintf("Prerender.OmniboxNavigationsCouldPrerender%s",
                         prerender::PrerenderManager::GetModeString()).c_str(),
      prerender::IsOmniboxEnabled(profile_));

  const AutocompleteMatch& match = log.result.match_at(log.selected_index);
  const GURL& opened_url = match.destination_url;
  const base::string16 lower_user_text(base::i18n::ToLower(log.text));

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
        row.id = base::GenerateGUID();
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

  if (table_.get()) {
    content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
        base::Bind(&AutocompleteActionPredictorTable::AddAndUpdateRows,
                   table_, rows_to_add, rows_to_update));
  }
}

void AutocompleteActionPredictor::CreateCaches(
    std::vector<AutocompleteActionPredictorTable::Row>* rows) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!profile_->IsOffTheRecord());
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
      HistoryServiceFactory::GetForProfile(profile_, Profile::EXPLICIT_ACCESS);
  if (!TryDeleteOldEntries(history_service)) {
    // Wait for the notification that the history service is ready and the URL
    // DB is loaded.
    notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_LOADED,
                                content::Source<Profile>(profile_));
  }
}

bool AutocompleteActionPredictor::TryDeleteOldEntries(HistoryService* service) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!profile_->IsOffTheRecord());
  DCHECK(!initialized_);

  if (!service)
    return false;

  history::URLDatabase* url_db = service->InMemoryDatabase();
  if (!url_db)
    return false;

  DeleteOldEntries(url_db);
  return true;
}

void AutocompleteActionPredictor::DeleteOldEntries(
    history::URLDatabase* url_db) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!profile_->IsOffTheRecord());
  DCHECK(!initialized_);
  DCHECK(table_.get());

  std::vector<AutocompleteActionPredictorTable::Row::Id> ids_to_delete;
  DeleteOldIdsFromCaches(url_db, &ids_to_delete);

  content::BrowserThread::PostTask(content::BrowserThread::DB, FROM_HERE,
      base::Bind(&AutocompleteActionPredictorTable::DeleteRows, table_,
                 ids_to_delete));

  FinishInitialization();
  if (incognito_predictor_)
    incognito_predictor_->CopyFromMainProfile();
}

void AutocompleteActionPredictor::DeleteOldIdsFromCaches(
    history::URLDatabase* url_db,
    std::vector<AutocompleteActionPredictorTable::Row::Id>* id_list) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!profile_->IsOffTheRecord());
  DCHECK(!initialized_);
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

void AutocompleteActionPredictor::CopyFromMainProfile() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(profile_->IsOffTheRecord());
  DCHECK(!initialized_);
  DCHECK(main_profile_predictor_);
  DCHECK(main_profile_predictor_->initialized_);

  db_cache_ = main_profile_predictor_->db_cache_;
  db_id_cache_ = main_profile_predictor_->db_id_cache_;
  FinishInitialization();
}

void AutocompleteActionPredictor::FinishInitialization() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!initialized_);

  // Incognito and normal profiles should listen only to omnibox notifications
  // from their own profile, but both should listen to history deletions from
  // the main profile, since opening the history page in either case actually
  // opens the non-incognito history (and lets users delete from there).
  notification_registrar_.Add(this, chrome::NOTIFICATION_OMNIBOX_OPENED_URL,
                              content::Source<Profile>(profile_));
  notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_URLS_DELETED,
      content::Source<Profile>(profile_->GetOriginalProfile()));
  initialized_ = true;
}

double AutocompleteActionPredictor::CalculateConfidence(
    const base::string16& user_text,
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

  const double number_of_hits = static_cast<double>(value.number_of_hits);
  return number_of_hits / (number_of_hits + value.number_of_misses);
}

AutocompleteActionPredictor::TransitionalMatch::TransitionalMatch() {
}

AutocompleteActionPredictor::TransitionalMatch::~TransitionalMatch() {
}

}  // namespace predictors
