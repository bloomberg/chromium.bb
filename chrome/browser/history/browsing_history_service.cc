// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/browsing_history_service.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/history/browsing_history_service_handler.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "extensions/features/features.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#endif

// The amount of time to wait for a response from the WebHistoryService.
static const int kWebHistoryTimeoutSeconds = 3;

namespace {

// Buckets for UMA histograms.
enum WebHistoryQueryBuckets {
  WEB_HISTORY_QUERY_FAILED = 0,
  WEB_HISTORY_QUERY_SUCCEEDED,
  WEB_HISTORY_QUERY_TIMED_OUT,
  NUM_WEB_HISTORY_QUERY_BUCKETS
};

// Returns true if |entry| represents a local visit that had no corresponding
// visit on the server.
bool IsLocalOnlyResult(const BrowsingHistoryService::HistoryEntry& entry) {
  return entry.entry_type == BrowsingHistoryService::HistoryEntry::LOCAL_ENTRY;
}

void RecordMetricsForNoticeAboutOtherFormsOfBrowsingHistory(bool shown) {
  UMA_HISTOGRAM_BOOLEAN(
      "History.ShownHeaderAboutOtherFormsOfBrowsingHistory",
      shown);
}

}  // namespace

BrowsingHistoryService::HistoryEntry::HistoryEntry(
    BrowsingHistoryService::HistoryEntry::EntryType entry_type,
    const GURL& url,
    const base::string16& title,
    base::Time time,
    const std::string& client_id,
    bool is_search_result,
    const base::string16& snippet,
    bool blocked_visit,
    base::Clock* clock) {
  this->entry_type = entry_type;
  this->url = url;
  this->title = title;
  this->time = time;
  this->client_id = client_id;
  all_timestamps.insert(time.ToInternalValue());
  this->is_search_result = is_search_result;
  this->snippet = snippet;
  this->blocked_visit = blocked_visit;
  this->clock = clock;
}

BrowsingHistoryService::HistoryEntry::HistoryEntry()
    : entry_type(EMPTY_ENTRY), is_search_result(false), blocked_visit(false) {
}

BrowsingHistoryService::HistoryEntry::HistoryEntry(const HistoryEntry& other) =
    default;

BrowsingHistoryService::HistoryEntry::~HistoryEntry() {
}

bool BrowsingHistoryService::HistoryEntry::SortByTimeDescending(
    const BrowsingHistoryService::HistoryEntry& entry1,
    const BrowsingHistoryService::HistoryEntry& entry2) {
  return entry1.time > entry2.time;
}

BrowsingHistoryService::QueryResultsInfo::QueryResultsInfo()
    : reached_beginning(false),
      has_synced_results(false) {}

BrowsingHistoryService::QueryResultsInfo::~QueryResultsInfo() {}

BrowsingHistoryService::BrowsingHistoryService(
    Profile* profile,
    BrowsingHistoryServiceHandler* handler)
    : has_pending_delete_request_(false),
      history_service_observer_(this),
      web_history_service_observer_(this),
      sync_service_observer_(this),
      has_synced_results_(false),
      has_other_forms_of_browsing_history_(false),
      profile_(profile),
      handler_(handler),
      clock_(new base::DefaultClock()),
      weak_factory_(this) {
  // Get notifications when history is cleared.
  history::HistoryService* local_history = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  if (local_history)
    history_service_observer_.Add(local_history);

  // Get notifications when web history is deleted.
  history::WebHistoryService* web_history =
      WebHistoryServiceFactory::GetForProfile(profile_);
  if (web_history) {
    web_history_service_observer_.Add(web_history);
  } else {
    // If |web_history| is not available, it means that the history sync is
    // disabled. Observe |sync_service| so that we can attach the listener
    // in case it gets enabled later.
    browser_sync::ProfileSyncService* sync_service =
        ProfileSyncServiceFactory::GetForProfile(profile_);
    if (sync_service)
      sync_service_observer_.Add(sync_service);
  }
}

BrowsingHistoryService::~BrowsingHistoryService() {
  query_task_tracker_.TryCancelAll();
  web_history_request_.reset();
}

void BrowsingHistoryService::OnStateChanged(syncer::SyncService* sync) {
  // If the history sync was enabled, start observing WebHistoryService.
  // This method should not be called after we already added the observer.
  history::WebHistoryService* web_history =
      WebHistoryServiceFactory::GetForProfile(profile_);
  if (web_history) {
    DCHECK(!web_history_service_observer_.IsObserving(web_history));
    web_history_service_observer_.Add(web_history);
    sync_service_observer_.RemoveAll();
  }
}

void BrowsingHistoryService::WebHistoryTimeout() {
  has_synced_results_ = false;
  // TODO(dubroy): Communicate the failure to the front end.
  if (!query_task_tracker_.HasTrackedTasks())
    ReturnResultsToHandler();

  UMA_HISTOGRAM_ENUMERATION(
      "WebHistory.QueryCompletion",
      WEB_HISTORY_QUERY_TIMED_OUT, NUM_WEB_HISTORY_QUERY_BUCKETS);
}

void BrowsingHistoryService::QueryHistory(
    const base::string16& search_text,
    const history::QueryOptions& options) {
  // Anything in-flight is invalid.
  query_task_tracker_.TryCancelAll();
  web_history_request_.reset();

  query_results_.clear();

  history::HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  hs->QueryHistory(search_text,
                   options,
                   base::Bind(&BrowsingHistoryService::QueryComplete,
                              base::Unretained(this),
                              search_text,
                              options),
                   &query_task_tracker_);

  history::WebHistoryService* web_history =
      WebHistoryServiceFactory::GetForProfile(profile_);

  // Set this to false until the results actually arrive.
  query_results_info_.has_synced_results = false;

  if (web_history) {
    web_history_query_results_.clear();
    web_history_request_ = web_history->QueryHistory(
        search_text,
        options,
        base::Bind(&BrowsingHistoryService::WebHistoryQueryComplete,
                   base::Unretained(this),
                   search_text, options,
                   base::TimeTicks::Now()));
    // Start a timer so we know when to give up.
    web_history_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kWebHistoryTimeoutSeconds),
        this, &BrowsingHistoryService::WebHistoryTimeout);

    // Test the existence of other forms of browsing history.
    browsing_data::ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
        ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_),
        web_history,
        base::Bind(
            &BrowsingHistoryService::OtherFormsOfBrowsingHistoryQueryComplete,
            weak_factory_.GetWeakPtr()));
  } else {
    // The notice could not have been shown, because there is no web history.
    RecordMetricsForNoticeAboutOtherFormsOfBrowsingHistory(false);
    has_synced_results_ = false;
    has_other_forms_of_browsing_history_ = false;
  }
}

void BrowsingHistoryService::RemoveVisits(
    std::vector<std::unique_ptr<BrowsingHistoryService::HistoryEntry>>* items) {
  if (delete_task_tracker_.HasTrackedTasks() ||
      has_pending_delete_request_ ||
      !profile_->GetPrefs()->GetBoolean(prefs::kAllowDeletingBrowserHistory)) {
    handler_->OnRemoveVisitsFailed();
    return;
  }

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  history::WebHistoryService* web_history =
      WebHistoryServiceFactory::GetForProfile(profile_);

  base::Time now = clock_->Now();
  std::vector<history::ExpireHistoryArgs> expire_list;
  expire_list.reserve(items->size());

  DCHECK(urls_to_be_deleted_.empty());
  for (auto it = items->begin(); it != items->end(); ++it) {
    // In order to ensure that visits will be deleted from the server and other
    // clients (even if they are offline), create a sync delete directive for
    // each visit to be deleted.
    sync_pb::HistoryDeleteDirectiveSpecifics delete_directive;
    sync_pb::GlobalIdDirective* global_id_directive =
    delete_directive.mutable_global_id_directive();
    history::ExpireHistoryArgs* expire_args = NULL;

    for (auto ts_iterator = it->get()->all_timestamps.begin();
         ts_iterator != it->get()->all_timestamps.end(); ++ts_iterator) {
      base::Time visit_time = base::Time::FromInternalValue(*ts_iterator);
      if (!expire_args) {
        GURL gurl(it->get()->url);
        expire_list.resize(expire_list.size() + 1);
        expire_args = &expire_list.back();
        expire_args->SetTimeRangeForOneDay(visit_time);
        expire_args->urls.insert(gurl);
        urls_to_be_deleted_.insert(gurl);
      }
      // The local visit time is treated as a global ID for the visit.
      global_id_directive->add_global_id(*ts_iterator);
    }

    // Set the start and end time in microseconds since the Unix epoch.
    global_id_directive->set_start_time_usec(
        (expire_args->begin_time - base::Time::UnixEpoch()).InMicroseconds());

    // Delete directives shouldn't have an end time in the future.
    // TODO(dubroy): Use sane time (crbug.com/146090) here when it's ready.
    base::Time end_time = std::min(expire_args->end_time, now);

    // -1 because end time in delete directives is inclusive.
    global_id_directive->set_end_time_usec(
        (end_time - base::Time::UnixEpoch()).InMicroseconds() - 1);

    // TODO(dubroy): Figure out the proper way to handle an error here.
    if (web_history)
      history_service->ProcessLocalDeleteDirective(delete_directive);
  }

  history_service->ExpireHistory(
      expire_list,
      base::Bind(&BrowsingHistoryService::RemoveComplete,
                 base::Unretained(this)),
      &delete_task_tracker_);

  if (web_history) {
    has_pending_delete_request_ = true;
    web_history->ExpireHistory(
        expire_list,
        base::Bind(&BrowsingHistoryService::RemoveWebHistoryComplete,
                   weak_factory_.GetWeakPtr()));
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // If the profile has activity logging enabled also clean up any URLs from
  // the extension activity log. The extension activity log contains URLS
  // which websites an extension has activity on so it will indirectly
  // contain websites that a user has visited.
  extensions::ActivityLog* activity_log =
      extensions::ActivityLog::GetInstance(profile_);
  for (std::vector<history::ExpireHistoryArgs>::const_iterator it =
       expire_list.begin(); it != expire_list.end(); ++it) {
    activity_log->RemoveURLs(it->urls);
  }
#endif

  for (const history::ExpireHistoryArgs& expire_entry : expire_list)
    AppBannerSettingsHelper::ClearHistoryForURLs(profile_, expire_entry.urls);
}

// static
void BrowsingHistoryService::MergeDuplicateResults(
    std::vector<BrowsingHistoryService::HistoryEntry>* results) {
  std::vector<BrowsingHistoryService::HistoryEntry> new_results;
  // Pre-reserve the size of the new vector. Since we're working with pointers
  // later on not doing this could lead to the vector being resized and to
  // pointers to invalid locations.
  new_results.reserve(results->size());
  // Maps a URL to the most recent entry on a particular day.
  std::map<GURL, BrowsingHistoryService::HistoryEntry*> current_day_entries;

  // Keeps track of the day that |current_day_urls| is holding the URLs for,
  // in order to handle removing per-day duplicates.
  base::Time current_day_midnight;

  std::sort(
      results->begin(), results->end(), HistoryEntry::SortByTimeDescending);

  for (std::vector<BrowsingHistoryService::HistoryEntry>::const_iterator it =
           results->begin(); it != results->end(); ++it) {
    // Reset the list of found URLs when a visit from a new day is encountered.
    if (current_day_midnight != it->time.LocalMidnight()) {
      current_day_entries.clear();
      current_day_midnight = it->time.LocalMidnight();
    }

    // Keep this visit if it's the first visit to this URL on the current day.
    if (current_day_entries.count(it->url) == 0) {
      new_results.push_back(*it);
      current_day_entries[it->url] = &new_results.back();
    } else {
      // Keep track of the timestamps of all visits to the URL on the same day.
      BrowsingHistoryService::HistoryEntry* entry =
          current_day_entries[it->url];
      entry->all_timestamps.insert(
          it->all_timestamps.begin(), it->all_timestamps.end());

      if (entry->entry_type != it->entry_type) {
        entry->entry_type =
            BrowsingHistoryService::HistoryEntry::COMBINED_ENTRY;
      }
    }
  }
  results->swap(new_results);
}

void BrowsingHistoryService::QueryComplete(
    const base::string16& search_text,
    const history::QueryOptions& options,
    history::QueryResults* results) {
  DCHECK_EQ(0U, query_results_.size());
  query_results_.reserve(results->size());

  for (size_t i = 0; i < results->size(); ++i) {
    history::URLResult const &page = (*results)[i];
    // TODO(dubroy): Use sane time (crbug.com/146090) here when it's ready.
    query_results_.push_back(
        HistoryEntry(
            HistoryEntry::LOCAL_ENTRY,
            page.url(),
            page.title(),
            page.visit_time(),
            std::string(),
            !search_text.empty(),
            page.snippet().text(),
            page.blocked_visit(),
            clock_.get()));
  }

  query_results_info_.search_text = search_text;
  query_results_info_.reached_beginning = results->reached_beginning();
  query_results_info_.start_time = options.begin_time;
  if (!options.end_time.is_null()) {
    query_results_info_.end_time =
        options.end_time - base::TimeDelta::FromDays(1);
  } else {
    query_results_info_.end_time = base::Time::Now();
  }

  if (!web_history_timer_.IsRunning())
    ReturnResultsToHandler();
}

void BrowsingHistoryService::ReturnResultsToHandler() {
  // Combine the local and remote results into |query_results_|, and remove
  // any duplicates.
  if (!web_history_query_results_.empty()) {
    int local_result_count = query_results_.size();
    query_results_.insert(query_results_.end(),
                          web_history_query_results_.begin(),
                          web_history_query_results_.end());
    MergeDuplicateResults(&query_results_);

    if (local_result_count) {
      // In the best case, we expect that all local results are duplicated on
      // the server. Keep track of how many are missing.
      int missing_count = std::count_if(
          query_results_.begin(), query_results_.end(), IsLocalOnlyResult);
      UMA_HISTOGRAM_PERCENTAGE("WebHistory.LocalResultMissingOnServer",
                               missing_count * 100.0 / local_result_count);
    }
  }

  handler_->OnQueryComplete(&query_results_, &query_results_info_);
  handler_->HasOtherFormsOfBrowsingHistory(
      has_other_forms_of_browsing_history_, has_synced_results_);

  query_results_.clear();
  web_history_query_results_.clear();
}

void BrowsingHistoryService::WebHistoryQueryComplete(
    const base::string16& search_text,
    const history::QueryOptions& options,
    base::TimeTicks start_time,
    history::WebHistoryService::Request* request,
    const base::DictionaryValue* results_value) {
  base::TimeDelta delta = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES("WebHistory.ResponseTime", delta);

  // If the response came in too late, do nothing.
  // TODO(dubroy): Maybe show a banner, and prompt the user to reload?
  if (!web_history_timer_.IsRunning())
    return;
  web_history_timer_.Stop();

  UMA_HISTOGRAM_ENUMERATION(
      "WebHistory.QueryCompletion",
      results_value ? WEB_HISTORY_QUERY_SUCCEEDED : WEB_HISTORY_QUERY_FAILED,
      NUM_WEB_HISTORY_QUERY_BUCKETS);

  DCHECK_EQ(0U, web_history_query_results_.size());
  const base::ListValue* events = NULL;
  if (results_value && results_value->GetList("event", &events)) {
    web_history_query_results_.reserve(events->GetSize());
    for (unsigned int i = 0; i < events->GetSize(); ++i) {
      const base::DictionaryValue* event = NULL;
      const base::DictionaryValue* result = NULL;
      const base::ListValue* results = NULL;
      const base::ListValue* ids = NULL;
      base::string16 url;
      base::string16 title;
      base::Time visit_time;

      if (!(events->GetDictionary(i, &event) &&
          event->GetList("result", &results) &&
          results->GetDictionary(0, &result) &&
          result->GetString("url", &url) &&
          result->GetList("id", &ids) &&
          ids->GetSize() > 0)) {
        continue;
      }

      // Ignore any URLs that should not be shown in the history page.
      GURL gurl(url);
      if (!CanAddURLToHistory(gurl))
        continue;

      // Title is optional, so the return value is ignored here.
      result->GetString("title", &title);

      // Extract the timestamps of all the visits to this URL.
      // They are referred to as "IDs" by the server.
      for (int j = 0; j < static_cast<int>(ids->GetSize()); ++j) {
        const base::DictionaryValue* id = NULL;
        std::string timestamp_string;
        int64_t timestamp_usec = 0;

        if (!ids->GetDictionary(j, &id) ||
            !id->GetString("timestamp_usec", &timestamp_string) ||
            !base::StringToInt64(timestamp_string, &timestamp_usec)) {
          NOTREACHED() << "Unable to extract timestamp.";
          continue;
        }
        // The timestamp on the server is a Unix time.
        base::Time time = base::Time::UnixEpoch() +
            base::TimeDelta::FromMicroseconds(timestamp_usec);

        // Get the ID of the client that this visit came from.
        std::string client_id;
        id->GetString("client_id", &client_id);

        web_history_query_results_.push_back(
            HistoryEntry(
                HistoryEntry::REMOTE_ENTRY,
                gurl,
                title,
                time,
                client_id,
                !search_text.empty(),
                base::string16(),
                /* blocked_visit */ false,
                clock_.get()));
      }
    }
  }
  has_synced_results_ = results_value != nullptr;
  query_results_info_.has_synced_results = has_synced_results_;
  if (!query_task_tracker_.HasTrackedTasks())
    ReturnResultsToHandler();
}

void BrowsingHistoryService::OtherFormsOfBrowsingHistoryQueryComplete(
    bool found_other_forms_of_browsing_history) {
  has_other_forms_of_browsing_history_ = found_other_forms_of_browsing_history;

  RecordMetricsForNoticeAboutOtherFormsOfBrowsingHistory(
      has_other_forms_of_browsing_history_);

  handler_->HasOtherFormsOfBrowsingHistory(
      has_other_forms_of_browsing_history_, has_synced_results_);
}

void BrowsingHistoryService::RemoveComplete() {
  urls_to_be_deleted_.clear();

  // Notify the handler that the deletion request is complete, but only if
  // web history delete request is not still pending.
  if (!has_pending_delete_request_)
    handler_->OnRemoveVisitsComplete();
}

void BrowsingHistoryService::RemoveWebHistoryComplete(bool success) {
  has_pending_delete_request_ = false;
  // TODO(dubroy): Should we handle failure somehow? Delete directives will
  // ensure that the visits are eventually deleted, so maybe it's not necessary.
  if (!delete_task_tracker_.HasTrackedTasks())
    RemoveComplete();
}

// Helper function for Observe that determines if there are any differences
// between the URLs noticed for deletion and the ones we are expecting.
static bool DeletionsDiffer(const history::URLRows& deleted_rows,
                            const std::set<GURL>& urls_to_be_deleted) {
  if (deleted_rows.size() != urls_to_be_deleted.size())
    return true;
  for (const auto& i : deleted_rows) {
    if (urls_to_be_deleted.find(i.url()) == urls_to_be_deleted.end())
      return true;
  }
  return false;
}

void BrowsingHistoryService::OnURLsDeleted(
    history::HistoryService* history_service,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  if (all_history || DeletionsDiffer(deleted_rows, urls_to_be_deleted_))
    handler_->HistoryDeleted();
}

void BrowsingHistoryService::OnWebHistoryDeleted() {
  // TODO(calamity): Only ignore web history deletions when they are actually
  // initiated by us, rather than ignoring them whenever we are deleting.
  if (!has_pending_delete_request_)
    handler_->HistoryDeleted();
}
