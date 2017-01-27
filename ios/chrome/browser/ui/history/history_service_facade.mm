// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/history/history_service_facade.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/mac/bind_objc_block.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browsing_data/core/history_notice_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/web_history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/query_parser/snippet.h"
#include "components/sync/protocol/history_delete_directive_specifics.pb.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/chrome/browser/history/history_utils.h"
#include "ios/chrome/browser/history/web_history_service_factory.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/sync/ios_chrome_profile_sync_service_factory.h"
#include "ios/chrome/browser/ui/history/history_entry.h"
#include "ios/chrome/browser/ui/history/history_service_facade_delegate.h"
#include "ios/chrome/browser/ui/history/history_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
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
bool IsLocalOnlyResult(const history::HistoryEntry& entry) {
  return entry.entry_type == history::HistoryEntry::LOCAL_ENTRY;
}

// Returns true if there are any differences between the URLs observed deleted
// and the ones we are expecting to be deleted.
static bool DeletionsDiffer(const history::URLRows& observed_deletions,
                            const std::set<GURL>& expected_deletions) {
  if (observed_deletions.size() != expected_deletions.size())
    return true;
  for (const auto& i : observed_deletions) {
    if (expected_deletions.find(i.url()) == expected_deletions.end())
      return true;
  }
  return false;
}

}  // namespace

#pragma mark - QueryResult

HistoryServiceFacade::QueryResult::QueryResult()
    : query(base::string16()),
      query_start_time(base::string16()),
      query_end_time(base::string16()),
      finished(false),
      sync_returned(false),
      has_synced_results(false),
      sync_finished(false),
      entries(std::vector<history::HistoryEntry>()) {}

HistoryServiceFacade::QueryResult::QueryResult(const QueryResult& other) =
    default;

HistoryServiceFacade::QueryResult::~QueryResult() {}

#pragma mark - RemovedEntry

HistoryServiceFacade::RemovedEntry::RemovedEntry(const GURL& url,
                                                 const base::Time& timestamp)
    : url(url) {
  timestamps = std::vector<base::Time>();
  timestamps.push_back(timestamp);
}

HistoryServiceFacade::RemovedEntry::RemovedEntry(
    const GURL& url,
    const std::vector<base::Time>& timestamps)
    : url(url), timestamps(timestamps) {}

HistoryServiceFacade::RemovedEntry::RemovedEntry(const RemovedEntry& other) =
    default;

HistoryServiceFacade::RemovedEntry::~RemovedEntry() {}

#pragma mark - HistoryServiceFacade

HistoryServiceFacade::HistoryServiceFacade(
    ios::ChromeBrowserState* browser_state,
    id<HistoryServiceFacadeDelegate> delegate)
    : has_pending_delete_request_(false),
      history_service_observer_(this),
      browser_state_(browser_state),
      delegate_(delegate),
      weak_factory_(this) {
  // Register as observer of HistoryService.
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS);
  if (history_service)
    history_service_observer_.Add(history_service);
}

HistoryServiceFacade::~HistoryServiceFacade() {
  query_task_tracker_.TryCancelAll();
  web_history_request_.reset();
  delegate_ = nil;
}

void HistoryServiceFacade::QueryHistory(const base::string16& search_text,
                                        const history::QueryOptions& options) {
  // Anything in-flight is invalid.
  query_task_tracker_.TryCancelAll();
  web_history_request_.reset();

  // Reset results.
  query_results_.clear();
  results_info_value_ = QueryResult();

  // Query synced history.
  history::WebHistoryService* web_history =
      ios::WebHistoryServiceFactory::GetForBrowserState(browser_state_);
  if (web_history) {
    web_history_query_results_.clear();
    web_history_request_ = web_history->QueryHistory(
        search_text, options,
        base::Bind(&HistoryServiceFacade::WebHistoryQueryComplete,
                   base::Unretained(this), search_text, options,
                   base::TimeTicks::Now()));
    // Start a timer so we know when to give up.
    web_history_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kWebHistoryTimeoutSeconds),
        this, &HistoryServiceFacade::WebHistoryTimeout);
  }

  // Query local history.
  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
  if (history_service) {
    history_service->QueryHistory(
        search_text, options,
        base::Bind(&HistoryServiceFacade::QueryComplete, base::Unretained(this),
                   search_text, options),
        &query_task_tracker_);
  }
}

void HistoryServiceFacade::RemoveHistoryEntries(
    const std::vector<RemovedEntry>& entries) {
  // Early return if there is a deletion in progress.
  if (delete_task_tracker_.HasTrackedTasks() || has_pending_delete_request_) {
    return;
  }

  history::HistoryService* history_service =
      ios::HistoryServiceFactory::GetForBrowserState(
          browser_state_, ServiceAccessType::EXPLICIT_ACCESS);
  history::WebHistoryService* web_history =
      ios::WebHistoryServiceFactory::GetForBrowserState(browser_state_);

  base::Time now = base::Time::Now();
  std::vector<history::ExpireHistoryArgs> expire_list;
  expire_list.reserve(entries.size());

  DCHECK(urls_to_be_deleted_.empty());
  for (const RemovedEntry& entry : entries) {
    GURL url = entry.url;
    DCHECK(entry.timestamps.size() > 0);

    // In order to ensure that visits will be deleted from the server and other
    // clients (even if they are offline), create a sync delete directive for
    // each visit to be deleted.
    sync_pb::HistoryDeleteDirectiveSpecifics delete_directive;
    sync_pb::GlobalIdDirective* global_id_directive =
        delete_directive.mutable_global_id_directive();

    expire_list.resize(expire_list.size() + 1);
    history::ExpireHistoryArgs* expire_args = &expire_list.back();
    expire_args->SetTimeRangeForOneDay(entry.timestamps.front());
    expire_args->urls.insert(entry.url);
    urls_to_be_deleted_.insert(entry.url);

    for (base::Time visit_time : entry.timestamps) {
      // The local visit time is treated as a global ID for the visit.
      global_id_directive->add_global_id(visit_time.ToInternalValue());
    }

    // Set the start and end time in microseconds since the Unix epoch.
    global_id_directive->set_start_time_usec(
        (expire_args->begin_time - base::Time::UnixEpoch()).InMicroseconds());

    // Delete directives shouldn't have an end time in the future.
    base::Time end_time = std::min(expire_args->end_time, now);

    // -1 because end time in delete directives is inclusive.
    global_id_directive->set_end_time_usec(
        (end_time - base::Time::UnixEpoch()).InMicroseconds() - 1);

    if (web_history)
      history_service->ProcessLocalDeleteDirective(delete_directive);
  }

  if (history_service) {
    history_service->ExpireHistory(
        expire_list, base::Bind(&HistoryServiceFacade::RemoveComplete,
                                base::Unretained(this)),
        &delete_task_tracker_);
  }

  if (web_history) {
    has_pending_delete_request_ = true;
    web_history->ExpireHistory(
        expire_list, base::Bind(&HistoryServiceFacade::RemoveWebHistoryComplete,
                                weak_factory_.GetWeakPtr()));
  }
}

void HistoryServiceFacade::QueryOtherFormsOfBrowsingHistory() {
  browser_sync::ProfileSyncService* sync_service =
      IOSChromeProfileSyncServiceFactory::GetForBrowserState(browser_state_);
  history::WebHistoryService* history_service =
      ios::WebHistoryServiceFactory::GetForBrowserState(browser_state_);
  browsing_data::ShouldShowNoticeAboutOtherFormsOfBrowsingHistory(
      sync_service, history_service,
      base::Bind(
          &HistoryServiceFacade::OtherFormsOfBrowsingHistoryQueryComplete,
          weak_factory_.GetWeakPtr()));
}

#pragma mark - Private methods

void HistoryServiceFacade::WebHistoryTimeout() {
  // If there are no outstanding tasks, send results to front end. Would also
  // be good to communicate the failure to the front end.
  if (!query_task_tracker_.HasTrackedTasks())
    ReturnResultsToFrontEnd();

  UMA_HISTOGRAM_ENUMERATION("WebHistory.QueryCompletion",
                            WEB_HISTORY_QUERY_TIMED_OUT,
                            NUM_WEB_HISTORY_QUERY_BUCKETS);
}

void HistoryServiceFacade::QueryComplete(const base::string16& search_text,
                                         const history::QueryOptions& options,
                                         history::QueryResults* results) {
  DCHECK_EQ(0U, query_results_.size());
  query_results_.reserve(results->size());

  for (history::URLResult* result : *results) {
    query_results_.push_back(history::HistoryEntry(
        history::HistoryEntry::LOCAL_ENTRY, result->url(), result->title(),
        result->visit_time(), std::string(), !search_text.empty(),
        result->snippet().text(), result->blocked_visit()));
  }

  results_info_value_.query = search_text;
  results_info_value_.finished = results->reached_beginning();
  results_info_value_.query_start_time =
      history::GetRelativeDateLocalized((options.begin_time));

  // Add the specific dates that were searched to display them.
  // Should put today if the start is in the future.
  if (!options.end_time.is_null()) {
    results_info_value_.query_end_time = history::GetRelativeDateLocalized(
        options.end_time - base::TimeDelta::FromDays(1));
  } else {
    results_info_value_.query_end_time =
        history::GetRelativeDateLocalized(base::Time::Now());
  }
  if (!web_history_timer_.IsRunning())
    ReturnResultsToFrontEnd();
}

void HistoryServiceFacade::WebHistoryQueryComplete(
    const base::string16& search_text,
    const history::QueryOptions& options,
    base::TimeTicks start_time,
    history::WebHistoryService::Request* request,
    const base::DictionaryValue* results_value) {
  base::TimeDelta delta = base::TimeTicks::Now() - start_time;
  UMA_HISTOGRAM_TIMES("WebHistory.ResponseTime", delta);

  // If the response came in too late, do nothing.
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
            result->GetString("url", &url) && result->GetList("id", &ids) &&
            ids->GetSize() > 0)) {
        LOG(WARNING) << "Improperly formed JSON response from history server.";
        continue;
      }

      // Ignore any URLs that should not be shown in the history page.
      GURL gurl(url);
      if (!ios::CanAddURLToHistory(gurl))
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

        web_history_query_results_.push_back(history::HistoryEntry(
            history::HistoryEntry::REMOTE_ENTRY, gurl, title, time, client_id,
            !search_text.empty(), base::string16(),
            /* blocked_visit */ false));
      }
    }
  }

  results_info_value_.has_synced_results = results_value != NULL;
  results_info_value_.sync_returned = true;
  if (results_value) {
    std::string continuation_token;
    results_value->GetString("continuation_token", &continuation_token);
    results_info_value_.sync_finished = continuation_token.empty();
  }
  if (!query_task_tracker_.HasTrackedTasks())
    ReturnResultsToFrontEnd();
}

void HistoryServiceFacade::RemoveComplete() {
  urls_to_be_deleted_.clear();

  // Notify the delegate that the deletion request is complete, but only if a
  // web history delete request is not still pending.
  if (has_pending_delete_request_)
    return;
  if ([delegate_ respondsToSelector:
                     @selector(historyServiceFacadeDidCompleteEntryRemoval:)]) {
    [delegate_ historyServiceFacadeDidCompleteEntryRemoval:this];
  }
}

void HistoryServiceFacade::RemoveWebHistoryComplete(bool success) {
  has_pending_delete_request_ = false;
  if (!delete_task_tracker_.HasTrackedTasks())
    RemoveComplete();
}

void HistoryServiceFacade::OtherFormsOfBrowsingHistoryQueryComplete(
    bool found_other_forms_of_browsing_history) {
  if ([delegate_ respondsToSelector:
                     @selector(historyServiceFacade:
                         shouldShowNoticeAboutOtherFormsOfBrowsingHistory:)]) {
    [delegate_ historyServiceFacade:this
        shouldShowNoticeAboutOtherFormsOfBrowsingHistory:
            found_other_forms_of_browsing_history];
  }
}

void HistoryServiceFacade::ReturnResultsToFrontEnd() {
  // Combine the local and remote results into |query_results_|, and remove
  // any duplicates.
  if (!web_history_query_results_.empty()) {
    int local_result_count = query_results_.size();
    query_results_.insert(query_results_.end(),
                          web_history_query_results_.begin(),
                          web_history_query_results_.end());
    history::MergeDuplicateHistoryEntries(&query_results_);

    if (local_result_count) {
      // In the best case, we expect that all local results are duplicated on
      // the server. Keep track of how many are missing.
      int missing_count = std::count_if(
          query_results_.begin(), query_results_.end(), IsLocalOnlyResult);
      UMA_HISTOGRAM_PERCENTAGE("WebHistory.LocalResultMissingOnServer",
                               missing_count * 100.0 / local_result_count);
    }
  }

  // Send results to delegate. Results may be empty.
  results_info_value_.entries = query_results_;
  if ([delegate_ respondsToSelector:@selector(historyServiceFacade:
                                             didReceiveQueryResult:)]) {
    [delegate_ historyServiceFacade:this
              didReceiveQueryResult:results_info_value_];
  }

  // Reset results variables.
  results_info_value_ = QueryResult();
  query_results_.clear();
  web_history_query_results_.clear();
}

void HistoryServiceFacade::OnURLsDeleted(
    history::HistoryService* history_service,
    bool all_history,
    bool expired,
    const history::URLRows& deleted_rows,
    const std::set<GURL>& favicon_urls) {
  if (all_history || DeletionsDiffer(deleted_rows, urls_to_be_deleted_)) {
    if ([delegate_
            respondsToSelector:
                @selector(historyServiceFacadeDidObserveHistoryDeletion:)]) {
      [delegate_ historyServiceFacadeDidObserveHistoryDeletion:this];
    }
  }
}
