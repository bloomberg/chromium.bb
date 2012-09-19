// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/resource_prefetch_predictor.h"

#include <map>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/predictors/predictor_database.h"
#include "chrome/browser/predictors/predictor_database_factory.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/load_from_memory_cache_details.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "net/base/mime_util.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context_getter.h"

using content::BrowserThread;

namespace {

// Don't store subresources whose Urls are longer than this.
size_t kMaxSubresourceUrlLengthBytes = 1000;

// For reporting whether a subresource is handled or not, and for what reasons.
enum ResourceStatus {
  RESOURCE_STATUS_HANDLED = 0,
  RESOURCE_STATUS_NOT_HTTP_PAGE = 1,
  RESOURCE_STATUS_NOT_HTTP_RESOURCE = 2,
  RESOURCE_STATUS_UNSUPPORTED_MIME_TYPE = 4,
  RESOURCE_STATUS_NOT_GET = 8,
  RESOURCE_STATUS_URL_TOO_LONG = 16,
  RESOURCE_STATUS_NOT_CACHEABLE = 32,
  RESOURCE_STATUS_HEADERS_MISSING = 64,
  RESOURCE_STATUS_MAX = 128,
};

// For reporting various interesting events that occur during the loading of a
// single main frame.
enum NavigationEvent {
  NAVIGATION_EVENT_REQUEST_STARTED = 0,
  NAVIGATION_EVENT_REQUEST_REDIRECTED = 1,
  NAVIGATION_EVENT_REQUEST_REDIRECTED_EMPTY_URL = 2,
  NAVIGATION_EVENT_REQUEST_EXPIRED = 3,
  NAVIGATION_EVENT_RESPONSE_STARTED = 4,
  NAVIGATION_EVENT_ONLOAD = 5,
  NAVIGATION_EVENT_ONLOAD_EMPTY_URL = 6,
  NAVIGATION_EVENT_ONLOAD_UNTRACKED_URL = 7,
  NAVIGATION_EVENT_ONLOAD_TRACKED_URL = 8,
  NAVIGATION_EVENT_SHOULD_TRACK_URL = 9,
  NAVIGATION_EVENT_SHOULD_NOT_TRACK_URL = 10,
  NAVIGATION_EVENT_URL_TABLE_FULL = 11,
  NAVIGATION_EVENT_HAVE_PREDICTIONS_FOR_URL = 12,
  NAVIGATION_EVENT_NO_PREDICTIONS_FOR_URL = 13,
  NAVIGATION_EVENT_COUNT = 14,
};

// For reporting events of interest that are not tied to any navigation.
enum ReportingEvent {
  REPORTING_EVENT_ALL_HISTORY_CLEARED = 0,
  REPORTING_EVENT_PARTIAL_HISTORY_CLEARED = 1,
  REPORTING_EVENT_COUNT = 2
};

void RecordNavigationEvent(NavigationEvent event) {
  UMA_HISTOGRAM_ENUMERATION("ResourcePrefetchPredictor.NavigationEvent",
                            event,
                            NAVIGATION_EVENT_COUNT);
}

}  // namespace

namespace predictors {

ResourcePrefetchPredictor::Config::Config()
    : max_navigation_lifetime_seconds(60),
      max_urls_to_track(500),
      min_url_visit_count(3),
      max_resources_per_entry(50),
      max_consecutive_misses(3) {
}

ResourcePrefetchPredictor::URLRequestSummary::URLRequestSummary()
    : resource_type(ResourceType::LAST_TYPE),
      was_cached(false) {
}

ResourcePrefetchPredictor::URLRequestSummary::URLRequestSummary(
    const URLRequestSummary& other)
        : navigation_id(other.navigation_id),
          resource_url(other.resource_url),
          resource_type(other.resource_type),
          mime_type(other.mime_type),
          was_cached(other.was_cached),
          redirect_url(other.redirect_url) {
}

ResourcePrefetchPredictor::URLRequestSummary::~URLRequestSummary() {
}

ResourcePrefetchPredictor::UrlTableCacheValue::UrlTableCacheValue() {
}

ResourcePrefetchPredictor::UrlTableCacheValue::~UrlTableCacheValue() {
}

ResourcePrefetchPredictor::ResourcePrefetchPredictor(
    const Config& config,
    Profile* profile)
    : profile_(profile),
      config_(config),
      initialization_state_(NOT_INITIALIZED),
      tables_(PredictorDatabaseFactory::GetForProfile(
          profile)->resource_prefetch_tables()) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  notification_registrar_.Add(this,
                              content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME,
                              content::NotificationService::AllSources());
}

ResourcePrefetchPredictor::~ResourcePrefetchPredictor() {
}

// static
bool ResourcePrefetchPredictor::IsEnabled(Profile* profile) {
  return prerender::IsSpeculativeResourcePrefetchingLearningEnabled(profile);
}

void ResourcePrefetchPredictor::LazilyInitialize() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK_EQ(initialization_state_, NOT_INITIALIZED);
  initialization_state_ = INITIALIZING;

  // Request the in-memory database from the history to force it to load so it's
  // available as soon as possible.
  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
  if (history_service)
    history_service->InMemoryDatabase();

  // Create local caches using the database as loaded.
  std::vector<UrlTableRow>* url_rows = new std::vector<UrlTableRow>();
  BrowserThread::PostTaskAndReply(
      BrowserThread::DB, FROM_HERE,
      base::Bind(&ResourcePrefetchPredictorTables::GetAllRows,
                 tables_, url_rows),
      base::Bind(&ResourcePrefetchPredictor::CreateCaches, AsWeakPtr(),
                 base::Owned(url_rows)));
}

void ResourcePrefetchPredictor::CreateCaches(
    std::vector<UrlTableRow>* url_rows) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DCHECK_EQ(initialization_state_, INITIALIZING);
  DCHECK(url_table_cache_.empty());
  DCHECK(inflight_navigations_.empty());

  // Copy the data to local caches.
  for (UrlTableRowVector::iterator it = url_rows->begin();
       it != url_rows->end(); ++it) {
    url_table_cache_[it->main_frame_url].rows.push_back(*it);
  }

  UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.UrlTableMainFrameUrlCount",
                       url_table_cache_.size());

  // Score and sort the database caches.
  // TODO(shishir): The following would be much more efficient if we used
  // pointers.
  for (UrlTableCacheMap::iterator it = url_table_cache_.begin();
       it != url_table_cache_.end(); ++it) {
    std::sort(it->second.rows.begin(),
              it->second.rows.end(),
              ResourcePrefetchPredictorTables::UrlTableRowSorter());
  }

  // Add notifications for history loading if it is not ready.
  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
    profile_, Profile::EXPLICIT_ACCESS);
  if (!history_service) {
    notification_registrar_.Add(this, chrome::NOTIFICATION_HISTORY_LOADED,
                                content::Source<Profile>(profile_));
  } else {
    OnHistoryAndCacheLoaded();
  }
}

// static
bool ResourcePrefetchPredictor::ShouldRecordRequest(
    net::URLRequest* request,
    ResourceType::Type resource_type) {
  return resource_type == ResourceType::MAIN_FRAME &&
      IsHandledMainPage(request);
}

// static
bool ResourcePrefetchPredictor::ShouldRecordResponse(
    net::URLRequest* response) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(response);
  if (!request_info)
    return false;

  return request_info->GetResourceType() == ResourceType::MAIN_FRAME ?
      IsHandledMainPage(response) : IsHandledSubresource(response);
}

// static
bool ResourcePrefetchPredictor::ShouldRecordRedirect(
    net::URLRequest* response) {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(response);
  if (!request_info)
    return false;

  return request_info->GetResourceType() == ResourceType::MAIN_FRAME &&
      IsHandledMainPage(response);
}

// static
bool ResourcePrefetchPredictor::IsHandledMainPage(net::URLRequest* request) {
  return request->original_url().scheme() == chrome::kHttpScheme;
}

// static
bool ResourcePrefetchPredictor::IsHandledSubresource(
    net::URLRequest* response) {
  int resource_status = 0;
  if (response->first_party_for_cookies().scheme() != chrome::kHttpScheme)
    resource_status |= RESOURCE_STATUS_NOT_HTTP_PAGE;

  if (response->original_url().scheme() != chrome::kHttpScheme)
    resource_status |= RESOURCE_STATUS_NOT_HTTP_RESOURCE;

  std::string mime_type;
  response->GetMimeType(&mime_type);
  if (!mime_type.empty() &&
      !net::IsSupportedImageMimeType(mime_type.c_str()) &&
      !net::IsSupportedJavascriptMimeType(mime_type.c_str()) &&
      !net::MatchesMimeType("text/css", mime_type)) {
    resource_status |= RESOURCE_STATUS_UNSUPPORTED_MIME_TYPE;
  }

  if (response->method() != "GET")
    resource_status |= RESOURCE_STATUS_NOT_GET;

  if (response->original_url().spec().length() >
      kMaxSubresourceUrlLengthBytes) {
    resource_status |= RESOURCE_STATUS_URL_TOO_LONG;
  }

  if (!response->response_info().headers)
    resource_status |= RESOURCE_STATUS_HEADERS_MISSING;

  if (!IsCacheable(response))
    resource_status |= RESOURCE_STATUS_NOT_CACHEABLE;

  UMA_HISTOGRAM_ENUMERATION("ResourcePrefetchPredictor.ResourceStatus",
                            resource_status,
                            RESOURCE_STATUS_MAX);

  return resource_status == 0;
}

// static
bool ResourcePrefetchPredictor::IsCacheable(const net::URLRequest* response) {
  if (response->was_cached())
    return true;

  // For non cached responses, we will ensure that the freshness lifetime is
  // some sane value.
  const net::HttpResponseInfo& response_info = response->response_info();
  if (!response_info.headers)
    return false;
  base::Time response_time(response_info.response_time);
  response_time += base::TimeDelta::FromSeconds(1);
  base::TimeDelta freshness = response_info.headers->GetFreshnessLifetime(
      response_time);
  return freshness > base::TimeDelta();
}

// static
ResourceType::Type ResourcePrefetchPredictor::GetResourceTypeFromMimeType(
    const std::string& mime_type,
    ResourceType::Type fallback) {
  if (net::IsSupportedImageMimeType(mime_type.c_str()))
    return ResourceType::IMAGE;
  else if (net::IsSupportedJavascriptMimeType(mime_type.c_str()))
    return ResourceType::SCRIPT;
  else if (net::MatchesMimeType("text/css", mime_type))
    return ResourceType::STYLESHEET;
  else
    return fallback;
}

void ResourcePrefetchPredictor::RecordURLRequest(
    const URLRequestSummary& request) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (initialization_state_ != INITIALIZED)
    return;

  CHECK_EQ(request.resource_type, ResourceType::MAIN_FRAME);
  OnMainFrameRequest(request);
}

void ResourcePrefetchPredictor::RecordUrlResponse(
    const URLRequestSummary& response) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (initialization_state_ != INITIALIZED)
    return;

  if (response.resource_type == ResourceType::MAIN_FRAME)
    OnMainFrameResponse(response);
  else
    OnSubresourceResponse(response);
}

void ResourcePrefetchPredictor::RecordUrlRedirect(
    const URLRequestSummary& response) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (initialization_state_ != INITIALIZED)
    return;

  CHECK_EQ(response.resource_type, ResourceType::MAIN_FRAME);
  OnMainFrameRedirect(response);
}

void ResourcePrefetchPredictor::OnMainFrameRequest(
    const URLRequestSummary& request) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(INITIALIZED, initialization_state_);

  RecordNavigationEvent(NAVIGATION_EVENT_REQUEST_STARTED);

  // Cleanup older navigations.
  CleanupAbandonedNavigations(request.navigation_id);

  // New empty navigation entry.
  inflight_navigations_.insert(std::make_pair(
      request.navigation_id, std::vector<URLRequestSummary>()));
}

void ResourcePrefetchPredictor::OnMainFrameResponse(
    const URLRequestSummary& response) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RecordNavigationEvent(NAVIGATION_EVENT_RESPONSE_STARTED);

  // TODO(shishir): The prefreshing will be stopped here.
}

void ResourcePrefetchPredictor::OnMainFrameRedirect(
    const URLRequestSummary& response) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RecordNavigationEvent(NAVIGATION_EVENT_REQUEST_REDIRECTED);

  // Remove the older navigation.
  inflight_navigations_.erase(response.navigation_id);

  // A redirect will not lead to another OnMainFrameRequest call, so record the
  // redirect url as a new navigation.

  // The redirect url may be empty if the url was invalid.
  if (response.redirect_url.is_empty()) {
    RecordNavigationEvent(NAVIGATION_EVENT_REQUEST_REDIRECTED_EMPTY_URL);
    return;
  }

  NavigationID navigation_id(response.navigation_id);
  navigation_id.main_frame_url = response.redirect_url;
  inflight_navigations_.insert(std::make_pair(
      navigation_id, std::vector<URLRequestSummary>()));
}

void ResourcePrefetchPredictor::OnSubresourceResponse(
    const URLRequestSummary& response) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (inflight_navigations_.find(response.navigation_id) ==
      inflight_navigations_.end()) {
    return;
  }

  inflight_navigations_[response.navigation_id].push_back(response);
}

void ResourcePrefetchPredictor::OnSubresourceLoadedFromMemory(
    const NavigationID& navigation_id,
    const GURL& resource_url,
    const std::string& mime_type,
    ResourceType::Type resource_type) {
  if (inflight_navigations_.find(navigation_id) == inflight_navigations_.end())
    return;

  URLRequestSummary summary;
  summary.navigation_id = navigation_id;
  summary.resource_url = resource_url;
  summary.mime_type = mime_type;
  summary.resource_type = GetResourceTypeFromMimeType(mime_type, resource_type);
  summary.was_cached = true;
  inflight_navigations_[navigation_id].push_back(summary);
}

void ResourcePrefetchPredictor::CleanupAbandonedNavigations(
    const NavigationID& navigation_id) {
  static const base::TimeDelta max_navigation_age =
      base::TimeDelta::FromSeconds(config_.max_navigation_lifetime_seconds);

  base::TimeTicks time_now = base::TimeTicks::Now();
  for (NavigationMap::iterator it = inflight_navigations_.begin();
       it != inflight_navigations_.end();) {
    if (it->first.IsSameRenderer(navigation_id) ||
        (time_now - it->first.creation_time > max_navigation_age)) {
      inflight_navigations_.erase(it++);
      RecordNavigationEvent(NAVIGATION_EVENT_REQUEST_EXPIRED);
    } else {
      ++it;
    }
  }
}

void ResourcePrefetchPredictor::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  switch (type) {
    case content::NOTIFICATION_LOAD_COMPLETED_MAIN_FRAME: {
      switch (initialization_state_) {
        case NOT_INITIALIZED:
          LazilyInitialize();
          break;
        case INITIALIZING:
          break;
        case INITIALIZED: {
          RecordNavigationEvent(NAVIGATION_EVENT_ONLOAD);
          const content::WebContents* web_contents =
              content::Source<content::WebContents>(source).ptr();
          NavigationID navigation_id(*web_contents);
          // WebContents can return an empty URL if the navigation entry
          // corresponding to the navigation has not been created yet.
          if (navigation_id.main_frame_url.is_empty())
            RecordNavigationEvent(NAVIGATION_EVENT_ONLOAD_EMPTY_URL);
          else
            OnNavigationComplete(navigation_id);
          break;
        }
        default:
          NOTREACHED() << "Unexpected initialization_state_: "
                       << initialization_state_;
      }
      break;
    }

    case content::NOTIFICATION_LOAD_FROM_MEMORY_CACHE: {
      const content::LoadFromMemoryCacheDetails* load_details =
          content::Details<content::LoadFromMemoryCacheDetails>(details).ptr();
      const content::WebContents* web_contents =
          content::Source<content::NavigationController>(
              source).ptr()->GetWebContents();

      NavigationID navigation_id(*web_contents);
      OnSubresourceLoadedFromMemory(navigation_id,
                                    load_details->url,
                                    load_details->mime_type,
                                    load_details->resource_type);
      break;
    }

    case chrome::NOTIFICATION_HISTORY_LOADED: {
      DCHECK_EQ(initialization_state_, INITIALIZING);
      notification_registrar_.Remove(this,
                                     chrome::NOTIFICATION_HISTORY_LOADED,
                                     content::Source<Profile>(profile_));
      OnHistoryAndCacheLoaded();
      break;
    }

    case chrome::NOTIFICATION_HISTORY_URLS_DELETED: {
      DCHECK_EQ(initialization_state_, INITIALIZED);
      const content::Details<const history::URLsDeletedDetails>
          urls_deleted_details =
              content::Details<const history::URLsDeletedDetails>(details);
      if (urls_deleted_details->all_history) {
        DeleteAllUrls();
        UMA_HISTOGRAM_ENUMERATION("ResourcePrefetchPredictor.ReportingEvent",
                                  REPORTING_EVENT_ALL_HISTORY_CLEARED,
                                  REPORTING_EVENT_COUNT);
      } else {
        DeleteUrls(urls_deleted_details->rows);
        UMA_HISTOGRAM_ENUMERATION("ResourcePrefetchPredictor.ReportingEvent",
                                  REPORTING_EVENT_PARTIAL_HISTORY_CLEARED,
                                  REPORTING_EVENT_COUNT);
      }
      break;
    }

    default:
      NOTREACHED() << "Unexpected notification observed.";
      break;
  }
}

void ResourcePrefetchPredictor::OnHistoryAndCacheLoaded() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK_EQ(initialization_state_, INITIALIZING);

  // Update the data with last visit info from in memory history db.
  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
  DCHECK(history_service);
  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  if (url_db) {
    std::vector<GURL> urls_to_delete;
    for (UrlTableCacheMap::iterator it = url_table_cache_.begin();
         it != url_table_cache_.end();) {
      history::URLRow url_row;
      if (url_db->GetRowForURL(it->first, &url_row) == 0) {
        urls_to_delete.push_back(it->first);
        url_table_cache_.erase(it++);
      } else {
        it->second.last_visit = url_row.last_visit();
        ++it;
      }
    }

    if (!urls_to_delete.empty()) {
      UMA_HISTOGRAM_COUNTS(
          "ResourcePrefetchPredictor.UrlTableMainFrameUrlsDeletedNotInHistory",
          urls_to_delete.size());
      UMA_HISTOGRAM_PERCENTAGE(
          "ResourcePrefetchPredictor."
              "UrlTableMainFrameUrlsDeletedNotInHistoryPercent",
          urls_to_delete.size() * 100.0 / url_table_cache_.size());

      BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
          base::Bind(&ResourcePrefetchPredictorTables::DeleteRowsForUrls,
                     tables_,
                     urls_to_delete));
    }
  }

  notification_registrar_.Add(this,
                              content::NOTIFICATION_LOAD_FROM_MEMORY_CACHE,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              chrome::NOTIFICATION_HISTORY_URLS_DELETED,
                              content::Source<Profile>(profile_));

  // TODO(shishir): Maybe listen for notifications for navigation being
  // abandoned and cleanup the inflight_navigations_.

  initialization_state_ = INITIALIZED;
}

bool ResourcePrefetchPredictor::ShouldTrackUrl(const GURL& url) {
  bool already_tracking = url_table_cache_.find(url) != url_table_cache_.end();

  HistoryService* history_service = HistoryServiceFactory::GetForProfile(
      profile_, Profile::EXPLICIT_ACCESS);
  DCHECK(history_service);
  history::URLDatabase* url_db = history_service->InMemoryDatabase();
  if (!url_db)
    return already_tracking;

  history::URLRow url_row;
  int visit_count = 0;
  if (url_db->GetRowForURL(url, &url_row) != 0)
    visit_count = url_row.visit_count();

  UMA_HISTOGRAM_COUNTS("ResourcePrefetchPredictor.HistoryVisitCountForUrl",
                       visit_count);

  return already_tracking || (visit_count >= config_.min_url_visit_count);
}

void ResourcePrefetchPredictor::OnNavigationComplete(
    const NavigationID& navigation_id) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (inflight_navigations_.find(navigation_id) ==
      inflight_navigations_.end()) {
    RecordNavigationEvent(NAVIGATION_EVENT_ONLOAD_UNTRACKED_URL);
    return;
  }

  RecordNavigationEvent(NAVIGATION_EVENT_ONLOAD_TRACKED_URL);

  // Report any stats.
  MaybeReportAccuracyStats(navigation_id);

  // Update the URL table.
  const GURL& main_frame_url = navigation_id.main_frame_url;
  if (ShouldTrackUrl(main_frame_url)) {
    RecordNavigationEvent(NAVIGATION_EVENT_SHOULD_TRACK_URL);
    LearnUrlNavigation(main_frame_url, inflight_navigations_[navigation_id]);
  } else {
    RecordNavigationEvent(NAVIGATION_EVENT_SHOULD_NOT_TRACK_URL);
  }

  // Remove the navigation.
  inflight_navigations_.erase(navigation_id);
}

void ResourcePrefetchPredictor::LearnUrlNavigation(
    const GURL& main_frame_url,
    const std::vector<URLRequestSummary>& new_resources) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (url_table_cache_.find(main_frame_url) == url_table_cache_.end()) {
    if (url_table_cache_.size() >= config_.max_urls_to_track)
      RemoveAnEntryFromUrlDB();

    url_table_cache_[main_frame_url].last_visit = base::Time::Now();
    int new_resources_size = static_cast<int>(new_resources.size());
    std::set<GURL> resources_seen;
    for (int i = 0; i < new_resources_size; ++i) {
      if (resources_seen.find(new_resources[i].resource_url) !=
          resources_seen.end()) {
        continue;
      }
      UrlTableRow row_to_add;
      row_to_add.main_frame_url = main_frame_url;
      row_to_add.resource_url = new_resources[i].resource_url;
      row_to_add.resource_type = new_resources[i].resource_type;
      row_to_add.number_of_hits = 1;
      row_to_add.average_position = i + 1;
      url_table_cache_[main_frame_url].rows.push_back(row_to_add);
      resources_seen.insert(new_resources[i].resource_url);
    }
  } else {
    UrlTableRowVector& old_resources = url_table_cache_[main_frame_url].rows;
    url_table_cache_[main_frame_url].last_visit = base::Time::Now();

    // Build indices over the data.
    std::map<GURL, int> new_index, old_index;
    int new_resources_size = static_cast<int>(new_resources.size());
    for (int i = 0; i < new_resources_size; ++i) {
      const URLRequestSummary& summary = new_resources[i];
      // Take the first occurence of every url.
      if (new_index.find(summary.resource_url) == new_index.end())
        new_index[summary.resource_url] = i;
    }
    int old_resources_size = static_cast<int>(old_resources.size());
    for (int i = 0; i < old_resources_size; ++i) {
      const UrlTableRow& row = old_resources[i];
      DCHECK(old_index.find(row.resource_url) == old_index.end());
      old_index[row.resource_url] = i;
    }

    // Go through the old urls and update their hit/miss counts.
    for (int i = 0; i < old_resources_size; ++i) {
      UrlTableRow& old_row = old_resources[i];
      if (new_index.find(old_row.resource_url) == new_index.end()) {
        ++old_row.number_of_misses;
        ++old_row.consecutive_misses;
      } else {
        const URLRequestSummary& new_row =
            new_resources[new_index[old_row.resource_url]];

        // Update the resource type since it could have changed.
        if (new_row.resource_type != ResourceType::LAST_TYPE)
          old_row.resource_type = new_row.resource_type;

        int position = new_index[old_row.resource_url] + 1;
        int total = old_row.number_of_hits + old_row.number_of_misses;
        old_row.average_position =
            ((old_row.average_position * total) + position) / (total + 1);
        ++old_row.number_of_hits;
        old_row.consecutive_misses = 0;
      }
    }

    // Add the new ones that we have not seen before.
    for (int i = 0; i < new_resources_size; ++i) {
      const URLRequestSummary& summary = new_resources[i];
      if (old_index.find(summary.resource_url) != old_index.end())
        continue;

      // Only need to add new stuff.
      UrlTableRow row_to_add;
      row_to_add.main_frame_url = main_frame_url;
      row_to_add.resource_url = summary.resource_url;
      row_to_add.resource_type = summary.resource_type;
      row_to_add.number_of_hits = 1;
      row_to_add.average_position = i + 1;
      old_resources.push_back(row_to_add);

      // To ensure we dont add the same url twice.
      old_index[summary.resource_url] = 0;
    }
  }

  // Trim and sort the rows after the update.
  UrlTableRowVector& rows = url_table_cache_[main_frame_url].rows;
  for (UrlTableRowVector::iterator it = rows.begin(); it != rows.end();) {
    it->UpdateScore();
    if (it->consecutive_misses >= config_.max_consecutive_misses)
      it = rows.erase(it);
    else
      ++it;
  }
  std::sort(rows.begin(), rows.end(),
            ResourcePrefetchPredictorTables::UrlTableRowSorter());
  if (static_cast<int>(rows.size()) > config_.max_resources_per_entry)
    rows.resize(config_.max_resources_per_entry);

  // If the row has no resources, remove it from the cache and delete the
  // entry in the database. Else only update the database.
  if (rows.size() == 0) {
    std::vector<GURL> urls_to_delete;
    urls_to_delete.push_back(main_frame_url);
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::DeleteRowsForUrls,
                   tables_,
                   urls_to_delete));
    url_table_cache_.erase(main_frame_url);
  } else {
    BrowserThread::PostTask(
        BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::UpdateRowsForUrl,
                   tables_,
                   main_frame_url,
                   rows));
  }
}

void ResourcePrefetchPredictor::RemoveAnEntryFromUrlDB() {
  if (url_table_cache_.empty())
    return;

  RecordNavigationEvent(NAVIGATION_EVENT_URL_TABLE_FULL);

  // TODO(shishir): Maybe use a heap to do this more efficiently.
  base::Time oldest_time;
  GURL url_to_erase;
  for (UrlTableCacheMap::iterator it = url_table_cache_.begin();
       it != url_table_cache_.end(); ++it) {
    if (url_to_erase.is_empty() || it->second.last_visit < oldest_time) {
      url_to_erase = it->first;
      oldest_time = it->second.last_visit;
    }
  }
  url_table_cache_.erase(url_to_erase);

  std::vector<GURL> urls_to_delete(1, url_to_erase);
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&ResourcePrefetchPredictorTables::DeleteRowsForUrls,
                 tables_,
                 urls_to_delete));
}

void ResourcePrefetchPredictor::MaybeReportAccuracyStats(
    const NavigationID& navigation_id) const {
  const GURL& main_frame_url = navigation_id.main_frame_url;
  DCHECK(inflight_navigations_.find(navigation_id) !=
         inflight_navigations_.end());

  bool have_predictions_for_url =
      url_table_cache_.find(main_frame_url) != url_table_cache_.end();
  if (have_predictions_for_url) {
    RecordNavigationEvent(NAVIGATION_EVENT_HAVE_PREDICTIONS_FOR_URL);
  } else {
    RecordNavigationEvent(NAVIGATION_EVENT_NO_PREDICTIONS_FOR_URL);
    return;
  }

  const std::vector<URLRequestSummary>& actual =
      inflight_navigations_.find(navigation_id)->second;
  const UrlTableRowVector& predicted =
      url_table_cache_.find(main_frame_url)->second.rows;

  std::map<GURL, bool> actual_resources;
  int from_network = 0;
  for (std::vector<URLRequestSummary>::const_iterator it = actual.begin();
       it != actual.end(); ++it) {
    actual_resources[it->resource_url] = it->was_cached;
    if (!it->was_cached)
      ++from_network;
  }

  // Measure the accuracy at 25, 50 predicted resources.
  ReportAccuracyHistograms(predicted, actual_resources, from_network, 25);
  ReportAccuracyHistograms(predicted, actual_resources, from_network, 50);
}

void ResourcePrefetchPredictor::ReportAccuracyHistograms(
    const UrlTableRowVector& predicted,
    const std::map<GURL, bool>& actual_resources,
    int total_resources_fetched_from_network,
    int max_assumed_prefetched) const {
  int prefetch_cached = 0, prefetch_network = 0, prefetch_missed = 0;
  int num_assumed_prefetched = std::min(static_cast<int>(predicted.size()),
                                        max_assumed_prefetched);
  if (num_assumed_prefetched == 0)
    return;

  for (int i = 0; i < num_assumed_prefetched; ++i) {
    const UrlTableRow& row = predicted[i];
    std::map<GURL, bool>::const_iterator it = actual_resources.find(
        row.resource_url);
    if (it == actual_resources.end()) {
      ++prefetch_missed;
    } else if (it->second) {
      ++prefetch_cached;
    } else {
      ++prefetch_network;
    }
  }

  std::string prefix = "ResourcePrefetchPredictor.Predicted";
  std::string suffix = "_" + base::IntToString(max_assumed_prefetched);

  // Macros to avoid using the STATIC_HISTOGRAM_POINTER_BLOCK in UMA_HISTOGRAM
  // definitions.
#define RPP_PREDICTED_HISTOGRAM_COUNTS(name, value) \
  { \
    std::string full_name = prefix + name + suffix; \
    base::Histogram* histogram = base::Histogram::FactoryGet( \
        full_name, 1, 1000000, 50, \
        base::Histogram::kUmaTargetedHistogramFlag); \
    histogram->Add(value); \
  }

#define RPP_PREDICTED_HISTOGRAM_PERCENTAGE(name, value) \
  { \
    std::string full_name = prefix + name + suffix; \
    base::Histogram* histogram = base::LinearHistogram::FactoryGet( \
        full_name, 1, 101, 102, base::Histogram::kUmaTargetedHistogramFlag); \
    histogram->Add(value); \
  }

  RPP_PREDICTED_HISTOGRAM_COUNTS("PrefetchCount", num_assumed_prefetched);
  RPP_PREDICTED_HISTOGRAM_COUNTS("PrefetchMisses_Count", prefetch_missed);
  RPP_PREDICTED_HISTOGRAM_COUNTS("PrefetchFromCache_Count", prefetch_cached);
  RPP_PREDICTED_HISTOGRAM_COUNTS("PrefetchFromNetwork_Count", prefetch_network);

  RPP_PREDICTED_HISTOGRAM_PERCENTAGE(
      "PrefetchMisses_PercentOfTotalPrefetched",
      prefetch_missed * 100.0 / num_assumed_prefetched);
  RPP_PREDICTED_HISTOGRAM_PERCENTAGE(
      "PrefetchFromCache_PercentOfTotalPrefetched",
      prefetch_cached * 100.0 / num_assumed_prefetched);
  RPP_PREDICTED_HISTOGRAM_PERCENTAGE(
      "PrefetchFromNetwork_PercentOfTotalPrefetched",
      prefetch_network * 100.0 / num_assumed_prefetched);

  // Measure the ratio of total number of resources prefetched from network vs
  // the total number of resources fetched by the page from the network.
  if (total_resources_fetched_from_network > 0) {
    RPP_PREDICTED_HISTOGRAM_PERCENTAGE(
        "PrefetchFromNetworkPercentOfTotalFromNetwork",
        prefetch_network * 100.0 / total_resources_fetched_from_network);
  }

#undef RPP_PREDICTED_HISTOGRAM_PERCENTAGE
#undef RPP_PREDICTED_HISTOGRAM_COUNTS
}

void ResourcePrefetchPredictor::DeleteAllUrls() {
  inflight_navigations_.clear();
  url_table_cache_.clear();

  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(&ResourcePrefetchPredictorTables::DeleteAllRows, tables_));
}

void ResourcePrefetchPredictor::DeleteUrls(const history::URLRows& urls) {
  std::vector<GURL> urls_to_delete;
  for (UrlTableCacheMap::iterator it = url_table_cache_.begin();
       it != url_table_cache_.end();) {
    if (std::find_if(urls.begin(), urls.end(),
                     history::URLRow::URLRowHasURL(it->first)) != urls.end()) {
      urls_to_delete.push_back(it->first);
      url_table_cache_.erase(it++);
    } else {
      ++it;
    }
  }

  if (!urls_to_delete.empty())
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&ResourcePrefetchPredictorTables::DeleteRowsForUrls,
                   tables_,
                   urls_to_delete));
}

void ResourcePrefetchPredictor::SetTablesForTesting(
    scoped_refptr<ResourcePrefetchPredictorTables> tables) {
  tables_ = tables;
}

}  // namespace predictors
